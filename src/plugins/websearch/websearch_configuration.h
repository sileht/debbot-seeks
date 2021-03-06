/**
 * The Seeks proxy and plugin framework are part of the SEEKS project.
 * Copyright (C) 2009-2011 Emmanuel Benazera <ebenazer@seeks-project.info>
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

#ifndef WEBSEARCH_CONFIGURATION_H
#define WEBSEARCH_CONFIGURATION_H

#include "configuration_spec.h"
#include "feeds.h"

#include <bitset>

using sp::configuration_spec;

namespace seeks_plugins
{

  class websearch_configuration : public configuration_spec
  {
    public:
      websearch_configuration(const std::string &filename);

      ~websearch_configuration();

      // virtual
      virtual void set_default_config();

      void set_default_engines();

      virtual void handle_config_cmd(char *cmd, const uint32_t &cmd_hash, char *arg,
                                     char *buf, const unsigned long &linenum);

      virtual void finalize_configuration();

      // main options.
      std::string _lang; /**< langage of the search results. */
      int _Nr; /**< max number of search results per page. */
      //std::bitset<NSEs> _se_enabled; /**< enabled search engines. */
      bool _default_engines; /**< whether this config defines the default engines or not. */
      feeds _se_enabled; /**< enabled search engines and feeds. */
      hash_map<const char*,feed_url_options,hash<const char*>,eqstr> _se_options; /**< search engines options. */
      feeds _se_default; /**< default set of search engines. */
      bool _thumbs; /**< enabled thumbs */
      bool _js; /**< enabled js */
      bool _content_analysis; /**< enables advanced ranking with background fetch of webpage content. */
      bool _clustering; /**< whether to enable clustering in the UI. XXX: probably always on when dev is finished. */

      // others.
      double _query_context_delay; /**< delay for query context before deletion, in seconds. */

      long _se_transfer_timeout; /**< transfer timeout when connecting to a search engine. */
      long _se_connect_timeout; /**< connection timeout when connecting to a search engine. */

      long _ct_transfer_timeout; /**< transfer timeout when fetching content for analysis & caching. */
      long _ct_connect_timeout;  /**< connection timeout when fetching content for analysis & caching. */
      int _max_expansions; /**< max number of allowed expansions. Prevents attacks. */

      bool _extended_highlight;

      std::string _background_proxy_addr; /**< address of a proxy through which to fetch URLs. */
      int _background_proxy_port; /** < proxy port. */
      bool _show_node_ip; /**< whether to show the node IP address when rendering the info bar. */
      bool _personalization; /**< whether to use personalized ranking. */
      std::string _result_message; /**< configurable message / warning to appear in a panel next to the results. */
      bool _dyn_ui; /**< user interface default, dynamic or static. */
      std::string _ui_theme; /**< User Interface theme. */
      short _num_reco_queries; /**< Max number of recommended queries returned / rendered. */
      int _num_recent_queries; /**< Max number of recent queries returned / rendered. */
  };

} /* end of namespace. */

#endif
