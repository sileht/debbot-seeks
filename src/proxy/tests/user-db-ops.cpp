/**
 * The Seeks proxy and plugin framework are part of the SEEKS project.
 * Copyright (C) 2011 Emmanuel Benazera, ebenazer@seeks-project.info
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

#include "user_db.h"
#include "user_db_fix.h"
#include "seeks_proxy.h"
#include "proxy_configuration.h"
#include "errlog.h"
#include "plugin_manager.h"

#include <iostream>
#include <fstream>
#include <stdlib.h>

using namespace sp;

std::string get_usage()
{
  std::string usage = "Usage: <db_file> <seeks base dir> <command>\n";
  usage += "commands:\n";
  usage += "remove_non_utf8\t\tRemoves non UTF8 queries and URLs\n";
  usage += "optimize\t\tOptimizes database, weeding out blank space";
  return usage;
}

int main(int argc, char **argv)
{
  if (argc < 4)
    {
      std::cout << get_usage() << std::endl;
      exit(0);
    }

  std::string dbfile = argv[1];
  std::string basedir = argv[2];
  std::string command = argv[3];

  if (command.empty())
    {
      std::cout << get_usage() << std::endl;
      exit(0);
    }

  //seeks_proxy::_configfile = basedir + "/config";
  seeks_proxy::initialize_mutexes();
  errlog::init_log_module();
  errlog::set_debug_level(LOG_LEVEL_FATAL | LOG_LEVEL_ERROR | LOG_LEVEL_INFO);

  seeks_proxy::_basedir = basedir.c_str();
  plugin_manager::_plugin_repository = basedir + "/plugins/";
  seeks_proxy::_config = new proxy_configuration(seeks_proxy::_configfile);
  seeks_proxy::_config->_user_db_file = dbfile;
  seeks_proxy::_config->_user_db_startup_check = false;
  seeks_proxy::_config->_user_db_optimize = false;

  seeks_proxy::_user_db = new user_db(dbfile);
  seeks_proxy::_user_db->open_db();

  plugin_manager::load_all_plugins();
  plugin_manager::instanciate_plugins();

  if (command == "remove_non_utf8")
    {
      seeks_proxy::_user_db->close_db();
      user_db_fix::fix_issue_154();
    }
  else if (command == "optimize")
    {
      seeks_proxy::_user_db->optimize_db();
      seeks_proxy::_user_db->close_db();
    }
  else
    {
      std::cout << "unknown command: " << command << std::endl;
      std::cout << get_usage() << std::endl;
      seeks_proxy::_user_db->close_db();
    }
}
