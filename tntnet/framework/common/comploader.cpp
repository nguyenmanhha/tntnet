/* comploader.cpp
   Copyright (C) 2003 Tommi Mäkitalo

This file is part of tntnet.

Tntnet is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Tntnet is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with tntnet; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330,
Boston, MA  02111-1307  USA
*/

#include <tnt/comploader.h>
#include <tnt/log.h>

namespace tnt
{

////////////////////////////////////////////////////////////////////////
// component_library
//
component* component_library::create(
  const std::string& component_name, comploader& cl,
  const urlmapper& rootmapper)
{
  creator_type creator;

  // look for creator in my map
  creatormap_type::const_iterator i = creatormap.find(component_name);
  if (i == creatormap.end())
  {
    // creatorsymbol not known - load it
    log_info("lookup symbol create_" << component_name);

    creator = (creator_type)sym(("create_" + component_name).c_str()).getSym();
    creatormap.insert(creatormap_type::value_type(component_name, creator));
  }
  else
    creator = i->second;

  // call the creator-function
  compident ci = compident(libname, component_name);
  log_info("create " << ci);
  return creator(ci, rootmapper, cl);
}

////////////////////////////////////////////////////////////////////////
// comploader
//
comploader::~comploader()
{
  for (componentmap_type::iterator i = componentmap.begin();
       i != componentmap.end(); ++i)
    i->second->drop();
}

RWLock comploader::libraryMonitor;
comploader::librarymap_type comploader::librarymap;
comploader::search_path_type comploader::search_path;

component& comploader::fetchComp(const compident& ci,
  const urlmapper& rootmapper)
{
  RdLock lock(componentMonitor);

  // lookup component
  componentmap_type::iterator it = componentmap.find(ci);
  if (it == componentmap.end())
  {
    // component not known - lookup shared lib, fetch creator and create a new
    // component
    lock.Unlock();

    WrLock wrlock(componentMonitor);

    // doublecheck after getting writelock
    it = componentmap.find(ci);
    if (it == componentmap.end())
    {
      component_library& lib = fetchLib(ci.libname);
      component* comp = lib.create(ci.compname, *this, rootmapper);

      // notify listener
      for (listener_container_type::iterator l = listener.begin();
           l != listener.end(); ++l)
        (*l)->onCreateComponent(lib, ci, *comp);

      componentmap[ci] = comp;
      comp->touch();
      return *comp;
    }
    else
    {
      it->second->touch();
      return *(it->second);
    }
  }
  else
  {
    it->second->touch();
    return *(it->second);
  }
}

component_library& comploader::fetchLib(const std::string& libname)
{
  RdLock lock(libraryMonitor);
  librarymap_type::iterator i = librarymap.find(libname);
  if (i == librarymap.end())
  {
    lock.Unlock();

    WrLock wrlock(libraryMonitor);

    // doublecheck after writelock
    librarymap_type::iterator i = librarymap.find(libname);
    if (i == librarymap.end())
    {
      // load library
      log_info("load library " << libname);
      component_library lib;

      bool found = false;
      for (search_path_type::const_iterator p = search_path.begin();
           p != search_path.end(); ++p)
      {
        try
        {
          log_debug("load library " << libname << " from " << *p << " dir");
          lib = component_library(*p, libname);
          found = true;
          break;
        }
        catch (const dl::dlopen_error&)
        {
        }
      }

      if (!found)
      {
        try
        {
          log_debug("load library " << libname << " from current dir");
          lib = component_library(".", libname);
        }
        catch (const dl::dlopen_error&)
        {
          log_debug("library in current dir not found - search lib-path");
          lib = component_library(libname);
        }
      }

      // notify listener
      for (listener_container_type::iterator l = listener.begin();
           l != listener.end(); ++l)
        (*l)->onLoadLibrary(lib);

      i = librarymap.insert(librarymap_type::value_type(libname, lib)).first;
    }

    return i->second;
  }
  else
    return i->second;
}

void comploader::addLoadLibraryListener(load_library_listener* l)
{
  listener.insert(l);
}

bool comploader::removeLoadLibraryListener(load_library_listener* l)
{
  return listener.erase(l) > 0;
}

void comploader::cleanup(unsigned seconds)
{
  time_t t = time(0) - seconds;

  RdLock rdlock(componentMonitor);
  for (componentmap_type::iterator it = componentmap.begin();
       it != componentmap.end(); ++it)
  {
    if (it->second->getLastAccesstime() < t)
    {
      // da ist was aufzur�umen
      rdlock.Unlock();

      WrLock wrlock(componentMonitor);
      while (it != componentmap.end())
      {
        if (it->second->getLastAccesstime() < t)
        {
          componentmap_type::iterator it2 = it;
          ++it;
          log_debug("drop " << it2->first);
          if (it2->second->drop())
            log_debug("deleted " << it2->first);
          componentmap.erase(it2);
        }
      else
        ++it;
      }
      break;
    }
  }
}

}
