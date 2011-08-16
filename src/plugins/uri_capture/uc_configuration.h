/**
 * The Seeks proxy and plugin framework are part of the SEEKS project.
 * Copyright (C) 2010 Emmanuel Benazera, ebenazer@seeks-project.info
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

#ifndef UC_CONFIGURATION_H
#define UC_CONFIGURATION_H

#include "configuration_spec.h"

using sp::configuration_spec;

namespace seeks_plugins
{

  class uc_configuration : public configuration_spec
  {
    public:
      uc_configuration(const std::string &filename);

      ~uc_configuration();

      // virtual
      virtual void set_default_config();

      virtual void handle_config_cmd(char *cmd, const uint32_t &cmd_hash, char *arg,
                                     char *buf, const unsigned long &linenum);

      virtual void finalize_configuration();

      // main options.
      time_t _sweep_cycle; /**< how long between two cycles of uri db record sweeping. */
      time_t _retention;   /**< uri db record retention, in seconds. */

      static uc_configuration *_config;
  };

} /* end of namespace. */

#endif
