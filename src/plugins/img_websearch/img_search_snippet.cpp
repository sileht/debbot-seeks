/**
 * The Seeks proxy and plugin framework are part of the SEEKS project.
 * Copyright (C) 2010 Emmanuel Benazera, juban@free.fr
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

#include "img_search_snippet.h"
#include "websearch.h"
#include "query_context.h"
#include "img_query_context.h"
#include "json_renderer.h"
#include "miscutil.h"
#include "mem_utils.h"
#include "encode.h"
#include "urlmatch.h"
#include <assert.h>

#if defined(PROTOBUF) && defined(TC)
#include "query_capture_configuration.h"
#endif

using sp::miscutil;
using sp::encode;
using sp::urlmatch;

namespace seeks_plugins
{

  img_search_snippet::img_search_snippet()
    :search_snippet()
#ifdef FEATURE_OPENCV2
    ,_surf_keypoints(NULL),_surf_descriptors(NULL),_surf_storage(NULL)
#endif
    , _cached_image(NULL)
  {
    _doc_type = IMAGE;
  }

  img_search_snippet::img_search_snippet(const short &rank)
    :search_snippet(rank)
#ifdef FEATURE_OPENCV2
    ,_surf_keypoints(NULL),_surf_descriptors(NULL)
#endif
    , _cached_image(NULL)
  {
    _doc_type = IMAGE;
#ifdef FEATURE_OPENCV2
    _surf_storage = cvCreateMemStorage(0);
#endif
  }

  img_search_snippet::~img_search_snippet()
  {
    if (_cached_image)
      delete _cached_image;

#ifdef FEATURE_OPENCV2
    if (_surf_keypoints)
      cvClearSeq(_surf_keypoints);
    if (_surf_descriptors)
      cvClearSeq(_surf_descriptors);
    if (_surf_storage)
      cvReleaseMemStorage(&_surf_storage);
#endif
  }

  std::string img_search_snippet::to_html_with_highlight(std::vector<std::string> &words,
      const std::string &base_url_str,
      const hash_map<const char*,const char*,hash<const char*>,eqstr> *parameters)
  {
    // check for URL redirection for capture & personalization of results.
    bool prs = true;
    const char *pers = miscutil::lookup(parameters,"prs");
    if (!pers)
      prs = websearch::_wconfig->_personalization;
    else
      {
        if (strcasecmp(pers,"on") == 0)
          prs = true;
        else if (strcasecmp(pers,"off") == 0)
          prs = false;
        else prs = websearch::_wconfig->_personalization;
      }

    std::string url = _url;

#if defined(PROTOBUF) && defined(TC)
    if (prs && websearch::_qc_plugin && websearch::_qc_plugin_activated
        && query_capture_configuration::_config
        && query_capture_configuration::_config->_mode_intercept == "redirect")
      {
        char *url_enc = encode::url_encode(url.c_str());
        url = base_url_str + "/qc_redir?q=" + _qc->_url_enc_query + "&amp;url=" + std::string(url_enc);
        free(url_enc);
      }
#endif

    std::string se_icon = "<span class=\"search_engine icon\" title=\"setitle\"><a href=\"" + base_url_str + "/search_img?q=@query@&amp;page=1&amp;expansion=1&amp;action=expand&amp;engines=seeng&amp;lang=" + _qc->_auto_lang + "&amp;ui=stat\">&nbsp;</a></span>";
    std::string html_content = "<li class=\"search_snippet search_snippet_img\">";

    html_content += "<h3><a href=\"";
    html_content += url + "\"><img src=\"";
    html_content += _cached;
    html_content += "\"></a><div>";

    const char *title_enc = encode::html_encode(_title.c_str());
    html_content += title_enc;
    free_const(title_enc);

    if (_img_engine.has_feed("google_img"))
      {
        std::string ggle_se_icon = se_icon;
        miscutil::replace_in_string(ggle_se_icon,"icon","search_engine_google");
        miscutil::replace_in_string(ggle_se_icon,"setitle","Google");
        miscutil::replace_in_string(ggle_se_icon,"seeng","google");
        miscutil::replace_in_string(ggle_se_icon,"@query@",_qc->_url_enc_query);
        html_content += ggle_se_icon;
      }
    if (_img_engine.has_feed("bing_img"))
      {
        std::string bing_se_icon = se_icon;
        miscutil::replace_in_string(bing_se_icon,"icon","search_engine_bing");
        miscutil::replace_in_string(bing_se_icon,"setitle","Bing");
        miscutil::replace_in_string(bing_se_icon,"seeng","bing");
        miscutil::replace_in_string(bing_se_icon,"@query@",_qc->_url_enc_query);
        html_content += bing_se_icon;
      }
    if (_img_engine.has_feed("flickr"))
      {
        std::string flickr_se_icon = se_icon;
        miscutil::replace_in_string(flickr_se_icon,"icon","search_engine_flickr");
        miscutil::replace_in_string(flickr_se_icon,"setitle","Flickr");
        miscutil::replace_in_string(flickr_se_icon,"seeng","flickr");
        miscutil::replace_in_string(flickr_se_icon,"@query@",_qc->_url_enc_query);
        html_content += flickr_se_icon;
      }
    if (_img_engine.has_feed("wcommons"))
      {
        std::string wcommons_se_icon = se_icon;
        miscutil::replace_in_string(wcommons_se_icon,"icon","search_engine_wcommons");
        miscutil::replace_in_string(wcommons_se_icon,"setitle","Wikimedia Commons");
        miscutil::replace_in_string(wcommons_se_icon,"seeng","wcommons");
        miscutil::replace_in_string(wcommons_se_icon,"@query@",_qc->_url_enc_query);
        html_content += wcommons_se_icon;
      }
    if (_img_engine.has_feed("yahoo_img"))
      {
        std::string yahoo_se_icon = se_icon;
        miscutil::replace_in_string(yahoo_se_icon,"icon","search_engine_yahoo");
        miscutil::replace_in_string(yahoo_se_icon,"setitle","yahoo");
        miscutil::replace_in_string(yahoo_se_icon,"seeng","yahoo");
        miscutil::replace_in_string(yahoo_se_icon,"@query@",_qc->_url_enc_query);
        html_content += yahoo_se_icon;
      }

    // XXX: personalization icon kind of look ugly with image snippets...
    /* if (_personalized)
      {
         html_content += "<h3 class=\"personalized_result personalized\" title=\"personalized result\">";
      }
    else */
    html_content += "</div></h3>";
    const char *cite_enc = NULL;
    if (!_cite.empty())
      {
        std::string cite_host;
        std::string cite_path;
        urlmatch::parse_url_host_and_path(_cite,cite_host,cite_path);
        cite_enc = encode::html_encode(cite_host.c_str());
      }
    else
      {
        std::string cite_host;
        std::string cite_path;
        urlmatch::parse_url_host_and_path(_url,cite_host,cite_path);
        cite_enc = encode::html_encode(cite_host.c_str());
      }
    std::string cite_enc_str = std::string(cite_enc);
    if (cite_enc_str.size()>4 && cite_enc_str.substr(0,4)=="www.") //TODO: tolower.
      cite_enc_str = cite_enc_str.substr(4);
    free_const(cite_enc);
    html_content += "<cite>";
    html_content += cite_enc_str;
    html_content += "</cite><br>";

    if (!_cached.empty())
      {
        char *enc_cached = encode::html_encode(_cached.c_str());
        miscutil::chomp(enc_cached);
        html_content += "<a class=\"search_cache\" href=\"";
        html_content += enc_cached;
        html_content += "\">Cached</a>";
        free_const(enc_cached);
      }

#ifdef FEATURE_OPENCV2
    if (!_sim_back)
      {
        set_similarity_link(parameters);
        html_content += "<a class=\"search_cache\" href=\"";
      }
    else
      {
        set_back_similarity_link(parameters);
        html_content += "<a class=\"search_similarity\" href=\"";
      }

    html_content += base_url_str + _sim_link;
    if (!_sim_back)
      html_content += "\">Similar</a>";
    else html_content += "\">Back</a>";
#endif
    html_content += "</li>\n";
    return html_content;
  }

  std::string img_search_snippet::to_json(const bool &thumbs,
                                          const std::vector<std::string> &query_words)
  {
    std::string json_str;
    json_str += "{";
    json_str += "\"id\":" + miscutil::to_string(_id) + ",";
    std::string title = _title;
    miscutil::replace_in_string(title,"\\","\\\\");
    miscutil::replace_in_string(title,"\"","\\\"");
    json_str += "\"title\":\"" + title + "\",";
    std::string url = _url;
    miscutil::replace_in_string(url,"\"","\\\"");
    json_str += "\"url\":\"" + url + "\",";
    std::string summary = _summary_noenc;
    miscutil::replace_in_string(summary,"\\","\\\\");
    miscutil::replace_in_string(summary,"\"","\\\"");
    json_str += "\"summary\":\"" + summary + "\",";
    json_str += "\"seeks_meta\":" + miscutil::to_string(_meta_rank) + ",";
    json_str += "\"seeks_score\":" + miscutil::to_string(_seeks_rank) + ",";
    double rank = _rank / static_cast<double>(_img_engine.count());
    json_str += "\"rank\":" + miscutil::to_string(rank) + ",";
    json_str += "\"cite\":\"";
    if (!_cite.empty())
      {
        std::string cite_host;
        std::string cite_path;
        urlmatch::parse_url_host_and_path(_cite,cite_host,cite_path);
        json_str += cite_host + "\",";
      }
    else
      {
        std::string cite_host;
        std::string cite_path;
        urlmatch::parse_url_host_and_path(_url,cite_host,cite_path);
        json_str += cite_host + "\",";
      }
    if (!_cached.empty())
      {
        std::string cached = _cached;
        miscutil::replace_in_string(cached,"\"","\\\"");
        json_str += "\"cached\":\"" + _cached + "\","; // XXX: cached might be malformed without preprocessing.
      }
    json_str += "\"engines\":[";
    json_str += json_renderer::render_engines(_img_engine);
    /*std::string json_str_eng = "";
    if (_img_engine.to_ulong()&SE_GOOGLE_IMG)
      json_str_eng += "\"google\"";
    if (_img_engine.to_ulong()&SE_BING_IMG)
      {
        if (!json_str_eng.empty())
          json_str_eng += ",";
        json_str_eng += "\"bing\"";
      }
    if (_img_engine.to_ulong()&SE_FLICKR)
      {
        if (!json_str_eng.empty())
          json_str_eng += ",";
        json_str_eng += "\"flickr\"";
      }
    if (_img_engine.to_ulong()&SE_WCOMMONS)
      {
        if (!json_str_eng.empty())
          json_str_eng += ",";
        json_str_eng += "\"wcommons\"";
      }
    if (_img_engine.to_ulong()&SE_YAHOO_IMG)
      {
        if (!json_str_eng.empty())
          json_str_eng += ",";
        json_str_eng += "\"yahoo\"";
    	}*/
    json_str += "]";
    json_str += ",\"type\":\"" + get_doc_type_str() + "\"";
    json_str += ",\"personalized\":\"";
    if (_personalized)
      json_str += "yes";
    else json_str += "no";
    json_str += "\"";
    if (!_date.empty())
      json_str += ",\"date\":\"" + _date + "\"";
    json_str += "}";
    return json_str;
  }

  bool img_search_snippet::is_se_enabled(const hash_map<const char*,const char*,hash<const char*>,eqstr> *parameters)
  {
    feeds se_enabled;
    img_query_context::fillup_img_engines(parameters,se_enabled);
    feeds band = _img_engine.inter(se_enabled);
    if (band.empty())
      band = _img_engine.inter_gen(se_enabled); // check for a wildcard (all feeds for a given parser).
    return (band.size() != 0);
  }

  void img_search_snippet::set_similarity_link(const hash_map<const char*,const char*,hash<const char*>,eqstr> *parameters)
  {
    const char *engines = miscutil::lookup(parameters,"engines");
    std::string sfsearch = "on";
    if (!static_cast<img_query_context*>(_qc)->_safesearch)
      sfsearch = "off";
    _sim_link = "/search_img?q=" + _qc->_url_enc_query
                + "&amp;page=1&amp;expansion=" + miscutil::to_string(_qc->_page_expansion)
                + "&amp;action=similarity&amp;safesearch=" + sfsearch
                + "&amp;id=" + miscutil::to_string(_id) + "&amp;engines=";
    if (engines)
      _sim_link += std::string(engines);
    _sim_back = false;
  }

  void img_search_snippet::set_back_similarity_link(const hash_map<const char*,const char*,hash<const char*>,eqstr> *parameters)
  {
    const char *engines = miscutil::lookup(parameters,"engines");
    std::string sfsearch = "on";
    if (!static_cast<img_query_context*>(_qc)->_safesearch)
      sfsearch = "off";
    _sim_link = "/search_img?q=" + _qc->_url_enc_query
                + "&amp;page=1&amp;expansion=" + miscutil::to_string(_qc->_page_expansion)
                + "&amp;action=expand&amp;safesearch=" + sfsearch + "&amp;engines=";
    if (engines)
      _sim_link += std::string(engines);
    _sim_back = true;
  }

  void img_search_snippet::merge_img_snippets(img_search_snippet *s1,
      const img_search_snippet *s2)
  {
    search_snippet::merge_snippets(s1,s2);

    if (s1->_img_engine.equal(s2->_img_engine))
      return;
    s1->_img_engine = s1->_img_engine.sunion(s2->_img_engine);

    /*std::bitset<IMG_NSEs> setest = s1->_img_engine;
    setest &= s2->_img_engine;
    if (setest.count()>0)
      return;
      s1->_img_engine |= s2->_img_engine; */

    if (!s1->_cached_image && s2->_cached_image)
      s1->_cached_image = new std::string(*s2->_cached_image);

    /// seeks rank.
    s1->_seeks_rank = s1->_img_engine.count();

    // XXX: hack, on English queries, Bing & Yahoo are the same engine,
    // therefore the rank must be tweaked accordingly in this special case.
    if (s1->_qc->_auto_lang == "en"
        && s1->_img_engine.has_feed("yahoo_img")
        && s1->_img_engine.has_feed("bing_img"))
      //&& (s1->_img_engine.to_ulong()&SE_YAHOO_IMG)
      //&& (s1->_img_engine.to_ulong()&SE_BING_IMG))
      s1->_seeks_rank--;
  }


} /* end of namespace. */
