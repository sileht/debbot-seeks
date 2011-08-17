/**
 * The Seeks proxy and plugin framework are part of the SEEKS project.
 * Copyright (C) 2009 Emmanuel Benazera, juban@free.fr
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "content_handler.h"
#include "mem_utils.h"
#include "curl_mget.h"
#include "html_txt_parser.h"
#include "websearch.h"
#include "oskmeans.h"
#include "seeks_proxy.h"
#include "proxy_configuration.h"
#include "encode.h"
#include "miscutil.h"
#include "errlog.h"

#include <pthread.h>
#include <iostream>
#include <stdexcept>

#include <assert.h>

using sp::curl_mget;
using sp::miscutil;
using sp::seeks_proxy;
using sp::proxy_configuration;
using sp::encode;
using sp::errlog;

namespace seeks_plugins
{
  int feature_thread_arg::_radius = 2;
  int feature_thread_arg::_step = 1;
  uint32_t feature_thread_arg::_window_length=2;

  int feature_tfidf_thread_arg::_radius = 1;
  int feature_tfidf_thread_arg::_step = 1;
  uint32_t feature_tfidf_thread_arg::_window_length=1;

  std::string** content_handler::fetch_snippets_content(const std::vector<std::string> &urls,
      const bool &proxy, const query_context *qc)
  {
    // just in case.
    if (urls.empty())
      return NULL;

    // fetch content.
    curl_mget cmg(urls.size(),websearch::_wconfig->_ct_connect_timeout,0,
                  websearch::_wconfig->_ct_transfer_timeout,0);
    std::vector<int> status;
    if (websearch::_wconfig->_background_proxy_addr.empty() && proxy)
      {
        if (seeks_proxy::_run_proxy)
          cmg.www_mget(urls,urls.size(),NULL,
                       seeks_proxy::_config->_haddr,seeks_proxy::_config->_hport,
                       status);
        else cmg.www_mget(urls,urls.size(),NULL,"",0,status);
      }
    else if (websearch::_wconfig->_background_proxy_addr.empty())
      cmg.www_mget(urls,urls.size(),NULL,"",0,status); // noproxy
    else cmg.www_mget(urls,urls.size(),NULL,
                        websearch::_wconfig->_background_proxy_addr,
                        websearch::_wconfig->_background_proxy_port,
                        status);

    std::string **outputs = new std::string*[urls.size()];
    int k = 0;
    for (size_t i=0; i<urls.size(); i++)
      {
        outputs[i] = NULL;
        if (cmg._outputs[i])
          {
            outputs[i] = cmg._outputs[i];
            k++;
          }
        else outputs[i] = NULL;
      }
    delete[] cmg._outputs;
    if (k == 0)
      {
        delete[] outputs;
        outputs = NULL;
      }
    return outputs;
  }

  void content_handler::fetch_all_snippets_summary_and_features(query_context *qc)
  {
    size_t nsnippets = qc->_cached_snippets.size();
    std::vector<std::string*> txt_contents;
    txt_contents.reserve(nsnippets);
    for (size_t i=0; i<nsnippets; i++)
      {
        if (qc->_cached_snippets.at(i)->_summary.empty()
            && qc->_cached_snippets.at(i)->_doc_type != TWEET
            && qc->_cached_snippets.at(i)->_doc_type != VIDEO_THUMB)
          {
            std::string *str = new std::string();
            txt_contents.push_back(str);
            continue;
          }

        // decode html.
        std::string dec_sum = qc->_cached_snippets.at(i)->_summary;
        if (qc->_cached_snippets.at(i)->_doc_type == TWEET
            || qc->_cached_snippets.at(i)->_doc_type == VIDEO_THUMB)
          dec_sum = qc->_cached_snippets.at(i)->_title;
        dec_sum = encode::html_decode(dec_sum);
        std::string *str = new std::string(dec_sum);
        txt_contents.push_back(str);
      }
    content_handler::extract_tfidf_features_from_snippets(qc,txt_contents,qc->_cached_snippets);
    for (size_t i=0; i<nsnippets; i++)
      if (txt_contents.at(i))
        delete txt_contents.at(i);
  }

  void content_handler::fetch_all_snippets_content_and_features(query_context *qc)
  {
    // prepare urls to fetch.
    size_t ns = qc->_cached_snippets.size();
    std::vector<std::string> urls;
    urls.reserve(ns);
    std::vector<search_snippet*> snippets;
    snippets.reserve(ns);
    for (size_t i=0; i<ns; i++)
      {
        search_snippet *sp = qc->_cached_snippets.at(i);
        if (sp->_cached_content)
          {
            //std::cerr << "[Debug]: already in cache: " << sp->_url << std::endl;
            continue;
          }
        urls.push_back(sp->_url);
        snippets.push_back(sp);
      }

    // fetch content.
    std::string **outputs = content_handler::fetch_snippets_content(urls,false,qc); // no proxy.
    if (!outputs)
      return;

    size_t nurls = urls.size();
    for (size_t i=0; i<nurls; i++)
      if (outputs[i])
        {
          search_snippet *sp = qc->get_cached_snippet(urls[i]);
          sp->_cached_content = outputs[i]; // cache fetched content.
        }

    // parse fetched content.
    std::string *txt_contents = content_handler::parse_snippets_txt_content(nurls,outputs);
    delete[] outputs; // contents still lives as cache within snippets.

    // compute features.
    std::vector<search_snippet*> sps;
    sps.reserve(nurls);
    std::vector<std::string*> valid_contents;
    valid_contents.reserve(nurls);
    for (size_t i=0; i<nurls; i++)
      if (!txt_contents[i].empty())
        {
          valid_contents.push_back(&txt_contents[i]);
          sps.push_back(snippets.at(i));
        }
    content_handler::extract_tfidf_features_from_snippets(qc,valid_contents,sps);
    delete[] txt_contents;
  }

  std::string* content_handler::parse_snippets_txt_content(const size_t &ncontents,
      std::string **outputs)
  {
    std::string *txt_outputs = new std::string[ncontents];

    pthread_t parser_threads[ncontents];
    html_txt_thread_arg* parser_args[ncontents];

    // threads, one per parser.
    for (size_t i=0; i<ncontents; i++)
      {
        if (outputs[i])
          {
            html_txt_thread_arg *args = new html_txt_thread_arg();
            args->_output = (char*)outputs[i]->c_str();
            if (!args->_output) // security check.
              {
                delete args;
                parser_threads[i] = 0;
                parser_args[i] = NULL;
                continue;
              }

            parser_args[i] = args;

            pthread_t ps_thread;
            int err = pthread_create(&ps_thread,NULL, // default attribute is PTHREAD_CREATE_JOINABLE
                                     (void * (*)(void *))content_handler::parse_output, args);
            if (err != 0)
              {
                errlog::log_error(LOG_LEVEL_ERROR, "Error creating parser thread.");
                parser_threads[i] = 0;
                delete args;
                parser_args[i] = NULL;
                continue;
              }

            parser_threads[i] = ps_thread;
          }
        else
          {
            parser_threads[i] = 0;
            parser_args[i] = NULL;
          }
      }

    // join threads.
    for (size_t i=0; i<ncontents; i++)
      {
        if (parser_threads[i] != 0)
          pthread_join(parser_threads[i],NULL);
      }

    for (size_t i=0; i<ncontents; i++)
      {
        if (parser_threads[i] != 0)
          {
            // debug: should go elsewhere, and be done more efficiently.
            miscutil::replace_in_string(parser_args[i]->_txt_content,"\t"," ");
            miscutil::replace_in_string(parser_args[i]->_txt_content,"\n"," ");
            miscutil::replace_in_string(parser_args[i]->_txt_content,"\r"," ");
            miscutil::replace_in_string(parser_args[i]->_txt_content,"\f"," ");
            miscutil::replace_in_string(parser_args[i]->_txt_content,"\v"," ");
            txt_outputs[i] = parser_args[i]->_txt_content;
            delete parser_args[i];
          }
      }

    return txt_outputs;
  }

  void content_handler::parse_output(html_txt_thread_arg &args)
  {
    // security check...
    if (!args._output)
      return;

    html_txt_parser *txt_parser = new html_txt_parser("");
    txt_parser->parse_output(args._output,NULL,0);
    args._txt_content = txt_parser->_txt;
    delete txt_parser;
  }

  void content_handler::extract_tfidf_features_from_snippets(query_context *qc,
      const std::vector<std::string*> &txt_contents,
      const std::vector<search_snippet*> &sps)
  {
    size_t ncontents = txt_contents.size();
    pthread_t feature_threads[ncontents];
    feature_tfidf_thread_arg* feature_args[ncontents];

    // XXX: limit to the number of threads.
    for  (size_t i=0; i<ncontents; i++)
      {
        hash_map<uint32_t,float,id_hash_uint> *vf = NULL;
        hash_map<uint32_t,std::string,id_hash_uint> *bow = NULL;
        try
          {
            vf = sps[i]->_features_tfidf;
            bow = sps[i]->_bag_of_words;
          }
        catch (std::out_of_range &oor)
          {
            break;
          }

        if (vf)
          {
            delete sps[i]->_features_tfidf;
            sps[i]->_features_tfidf = NULL;
            if (sps[i]->_bag_of_words)
              {
                delete sps[i]->_bag_of_words;
                sps[i]->_bag_of_words = NULL;
              }
            vf = NULL;
          }

        if (!vf)
          {
            vf = new hash_map<uint32_t,float,id_hash_uint>();
            bow = new hash_map<uint32_t,std::string,id_hash_uint>();
            feature_tfidf_thread_arg *args = new feature_tfidf_thread_arg(txt_contents[i],vf,
                bow,qc->_auto_lang);
            feature_args[i] = args;

            pthread_t f_thread;
            int err = pthread_create(&f_thread,NULL, // default attribute is PTHREAD_CREATE_JOINABLE
                                     (void*(*)(void*))content_handler::generate_features_tfidf,args);
            if (err != 0)
              {
                errlog::log_error(LOG_LEVEL_ERROR, "Error creating feature generator thread.");
                feature_threads[i] = 0;
                delete vf;
                delete bow;
                delete args;
                feature_args[i] = NULL;
                continue;
              }
            feature_threads[i] = f_thread;
          }
        else
          {
            feature_threads[i] = 0;
            feature_args[i] = NULL;
          }
      }

    // join threads.
    std::vector<hash_map<uint32_t,float,id_hash_uint>*> bags;
    bags.reserve(ncontents);
    for (size_t i=0; i<ncontents; i++)
      {
        if (feature_threads[i] != 0)
          {
            pthread_join(feature_threads[i],NULL);
            bags.push_back(feature_args[i]->_vf);
          }
      }

    mrf::compute_tf_idf(bags);

    for (size_t i=0; i<ncontents; i++)
      {
        if (feature_threads[i] != 0)
          {
            sps[i]->_features_tfidf = feature_args[i]->_vf; // cache features.
            sps[i]->_bag_of_words = feature_args[i]->_bow; // cache words.
            //std::cerr << "[Debug]: url: " << sps[i]->_url << " --> " << sps[i]->_features_tfidf->size() << " features.\n";
            delete feature_args[i];
          }
        else
          {
          }
      }
    qc->_compute_tfidf_features = false;
  }

  void content_handler::extract_features_from_snippets(query_context *qc,
      const std::vector<std::string*> &txt_contents,
      const std::vector<search_snippet*> &sps)
  {
    size_t ncontents = txt_contents.size();
    pthread_t feature_threads[ncontents];
    feature_thread_arg* feature_args[ncontents];

    // XXX: limits the number of threads.
    for  (size_t i=0; i<ncontents; i++)
      {
        std::vector<uint32_t> *vf = NULL;
        try
          {
            vf = sps[i]->_features;
          }
        catch (std::out_of_range &oor)
          {
            break;
          }

        if (!vf)
          {
            vf = new std::vector<uint32_t>();
            feature_thread_arg *args = new feature_thread_arg(txt_contents[i],vf);
            feature_args[i] = args;

            pthread_t f_thread;
            int err = pthread_create(&f_thread,NULL, // default attribute is PTHREAD_CREATE_JOINABLE
                                     (void*(*)(void*))content_handler::generate_features,args);
            if (err != 0)
              {
                errlog::log_error(LOG_LEVEL_ERROR, "Error creating feature generator thread.");
                feature_threads[i] = 0;
                delete args;
                feature_args[i] = NULL;
                continue;
              }
            feature_threads[i] = f_thread;
          }
        else
          {
            feature_threads[i] = 0;
            feature_args[i] = NULL;
          }
      }

    // join threads.
    for (size_t i=0; i<ncontents; i++)
      {
        if (feature_threads[i] != 0)
          pthread_join(feature_threads[i],NULL);
      }

    for (size_t i=0; i<ncontents; i++)
      {
        if (feature_threads[i] != 0)
          {
            sps[i]->_features = feature_args[i]->_vf; // cache features.
            //std::cerr << "[Debug]: url: " << sps[i]->_url << " --> " << sps[i]->_features->size() << " features.\n";
            delete feature_args[i];
          }
      }
  }

  void content_handler::generate_features(feature_thread_arg &args)
  {
    try
      {
        mrf::tokenize_and_mrf_features(*args._txt_content,mrf::_default_delims,*args._vf,
                                       feature_thread_arg::_radius,feature_thread_arg::_step,
                                       feature_thread_arg::_window_length);
        mrf::unique_features(*args._vf);
      }
    catch (std::exception &e)
      {
        errlog::log_error(LOG_LEVEL_ERROR,"Error %s generating unique features.", e.what());
      }
    catch (...)
      {
      }
  }

  void content_handler::generate_features_tfidf(feature_tfidf_thread_arg &args)
  {
    mrf::tokenize_and_mrf_features(*args._txt_content,mrf::_default_delims,*args._vf,args._bow,
                                   feature_tfidf_thread_arg::_radius,feature_tfidf_thread_arg::_step,
                                   feature_tfidf_thread_arg::_window_length,args._lang);
  }

  void content_handler::feature_based_similarity_scoring(query_context *qc,
      const size_t &nsps,
      search_snippet **sps,
      search_snippet *ref_sp) throw (sp_exception)
  {
    if (!ref_sp)
      {
        std::string msg = "No reference snippet for similarity computation";
        errlog::log_error(LOG_LEVEL_ERROR,msg.c_str());
        throw sp_exception(WB_ERR_NO_REF_SIM,msg);
      }
    // reference features.
    hash_map<uint32_t,float,id_hash_uint> *ref_features = ref_sp->_features_tfidf;
    if (!ref_features) // sometimes the content wasn't fetched, and features are not there.
      {
        std::string msg = "No reference snippet features to compute similarity from";
        errlog::log_error(LOG_LEVEL_ERROR,msg.c_str());
        throw sp_exception(WB_ERR_NO_REF_SIM,msg);
      }

    // compute scores.
    for (size_t i=0; i<nsps; i++)
      {
        if (sps[i]->_features_tfidf)
          {
            sps[i]->_seeks_ir = oskmeans::distance_normed_points(*ref_features,*sps[i]->_features_tfidf);
            /* std::cerr << "[Debug]: url: " << sps[i]->_url
              << " -- score: " << sps[i]->_seeks_ir << std::endl; */
          }
      }
  }

  bool content_handler::has_same_content(query_context *qc,
                                         search_snippet *sp1, search_snippet *sp2,
                                         const double &similarity_threshold)
  {
    static std::string token_delims = " \t\n";

    //TODO: referer.

    // we may already have some content in cache, let's check to not fetch it more than once.
    std::string url1 = sp1->_url;
    std::string url2 = sp2->_url;
    std::vector<std::string> urls;
    urls.reserve(2);

    bool parsing = true;
    if (sp1->_doc_type == TWEET && sp2->_doc_type == TWEET)
      parsing = false;
    std::string *content1 = (sp1->_doc_type == TWEET) ? &sp1->_title : sp1->_cached_content;
    std::string *content2 = (sp2->_doc_type == TWEET) ? &sp2->_title : sp2->_cached_content;

    std::string **outputs = NULL;
    if (!content1 && !content2)
      {
        urls.push_back(url1);
        urls.push_back(url2);
        outputs = content_handler::fetch_snippets_content(urls,false,qc); // no proxy.
        if (outputs)
          {
            sp1->_cached_content = outputs[0];
            sp2->_cached_content = outputs[1];
          }
      }
    else if (!content1)
      {
        outputs = new std::string*[2];
        urls.push_back(url1);
        std::string **output1 = content_handler::fetch_snippets_content(urls,false,qc); // no proxy.

        if (output1)
          {
            outputs[0] = *output1;
            delete output1;
            outputs[1] = content2;
            urls.push_back(url2);
            sp1->_cached_content = outputs[0];
          }
        else outputs[0] = NULL;
      }
    else if (!content2)
      {
        outputs = new std::string*[2];
        urls.push_back(url2);
        std::string **output2 = content_handler::fetch_snippets_content(urls,false,qc); // no proxy.
        outputs[0] = content1;
        if (output2)
          {
            outputs[1] = *output2;
            delete[] output2;
            urls.push_back(url1);
            sp2->_cached_content = outputs[1];
          }
        else outputs[1] = NULL;
      }
    else
      {
        outputs = new std::string*[2];
        outputs[0] = content1;
        outputs[1] = content2;
      }

    if (outputs == NULL
        || outputs[0] == NULL || outputs[1] == NULL)  // error handling.
      {
        // cleanup the shell, existing content remains in cache in the snippets.
        if (outputs)
          delete[] outputs;
        return false;
      }

    // parse content and keep text only.
    std::string *txt_contents = NULL;
    if (parsing)
      {
        txt_contents = content_handler::parse_snippets_txt_content(2,outputs);
      }
    else
      {
        txt_contents = new std::string[2];
        txt_contents[0] = *outputs[0];
        txt_contents[1] = *outputs[1];
      }
    delete[] outputs;
    outputs = NULL;

    if (txt_contents[0].empty() || txt_contents[1].empty())
      {
        delete[] txt_contents;
        return false;
      }

    // quick check for similarity.
    double rad = static_cast<double>(std::min(txt_contents[0].size(),txt_contents[1].size()))
                 / static_cast<double>(std::max(txt_contents[0].size(),txt_contents[1].size()));
    if (rad < similarity_threshold)
      {
        delete[] txt_contents;
        return false;
      }

    std::vector<search_snippet*> sps;
    sps.reserve(2);
    sps.push_back(sp1);
    sps.push_back(sp2);
    std::vector<std::string*> valid_content;
    valid_content.reserve(2);
    valid_content.push_back(&txt_contents[0]);
    valid_content.push_back(&txt_contents[1]);
    content_handler::extract_features_from_snippets(qc,valid_content,sps);
    delete[] txt_contents;

    // check for similarity.
    uint32_t common_features = 0;
    std::vector<uint32_t> *f1 = sp1->_features;
    std::vector<uint32_t> *f2 = sp2->_features;
    assert(f1!=NULL);
    assert(f2!=NULL);

    rad = mrf::radiance(*f1,*f2,common_features);
    double threshold = (common_features*common_features)
                       / ((1.0+similarity_threshold)*static_cast<double>(f1->size()+f2->size()-2*common_features)+mrf::_epsilon);
    bool result = false;
    if (rad >= threshold)
      result = true;

    // we don't need simple features anymore.
    delete sp1->_features;
    sp1->_features = NULL;
    delete sp2->_features;
    sp2->_features = NULL;

    /* std::cerr << "Radiance: " << rad << " -- threshold: " << threshold << " -- result: " << result << std::endl;
     std::cerr << "[Debug]: comparison result: " << result << std::endl; */

    return result;
  }

} /* end of namespace. */
