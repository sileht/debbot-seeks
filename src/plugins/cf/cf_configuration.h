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

#ifndef CF_CONFIGURATION_H
#define CF_CONFIGURATION_H

#include "configuration_spec.h"

using sp::configuration_spec;

namespace seeks_plugins
{

  class cf_configuration : public configuration_spec
  {
    public:
      cf_configuration(const std::string &filename);

      ~cf_configuration();

      // virtual
      virtual void set_default_config();

      virtual void handle_config_cmd(char *cmd, const uint32_t &cmd_hash, char *arg,
                                     char *buf, const unsigned long &linenum);

      virtual void finalize_configuration();

      // main options.
      float _domain_name_weight; /**< weight given to domain names. */
      int _record_cache_timeout; /**< timeout on cached remote records, in seconds. */

      static cf_configuration *_config;
  };

} /* end of namespace. */

#endif

