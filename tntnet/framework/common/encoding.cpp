/* encoding.cpp
   Copyright (C) 2003-2005 Tommi Maekitalo

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

#include "tnt/encoding.h"
#include <cxxtools/log.h>
#include <stdexcept>

log_define("tntnet.encoding");

namespace tnt
{
  void Encoding::parse(const std::string& header)
  {
    log_debug("encode header \"" << header << '"');

    enum {
      state_0,
      state_encoding,
      state_quality,
      state_qualitypoint,
      state_qualitytenth,
      state_qualityign
    } state = state_0;

    std::string encoding;
    unsigned quality;
    for (std::string::const_iterator it = header.begin(); it != header.end(); ++it)
    {
      char ch = *it;
      switch (state)
      {
        case state_0:
          if (!std::isspace(ch))
          {
            encoding = ch;
            state = state_encoding;
          }
          break;

        case state_encoding:
          if (ch == ';')
            state = state_quality;
          else if (ch == ',')
          {
            log_debug("encoding=" << encoding);
            encodingMap.insert(encodingMapType::value_type(encoding, 1));
            state = state_0;
          }
          else
            encoding += ch;
          break;

        case state_quality:
          if (std::isdigit(ch))
          {
            quality = (ch - '0') * 10;
            state = state_qualitypoint;
          }
          else
            throw std::runtime_error("invalid encoding-string \"" + header + '"');
          break;

        case state_qualitypoint:
          if (ch == '.')
            state = state_qualitytenth;
          else if (ch == ';')
          {
            log_debug("encoding=" << encoding << " quality " << quality);
            encodingMap.insert(encodingMapType::value_type(encoding, quality));
            state = state_0;
          }
          else
            throw std::runtime_error("invalid encoding-string \"" + header + '"');
          break;

        case state_qualitytenth:
          if (std::isdigit(ch))
          {
            quality += ch - '0';
            log_debug("encoding=" << encoding << " quality " << quality);
            encodingMap.insert(encodingMapType::value_type(encoding, quality));
            state = state_qualityign;
          }
          else if (ch == ';')
            state = state_0;
          break;

        case state_qualityign:
          if (ch == ';')
            state = state_0;
          break;
      }
    }

    switch (state)
    {
      case state_encoding:
        log_debug("encoding=" << encoding);
        encodingMap.insert(encodingMapType::value_type(encoding, 1));
        break;

      case state_quality:
      case state_qualitypoint:
      case state_qualitytenth:
        log_debug("encoding=" << encoding << " quality " << quality);
        encodingMap.insert(encodingMapType::value_type(encoding, quality));
        break;

      default:
        break;
    }
  }

  unsigned Encoding::accept(const std::string& encoding) const
  {
    // check, if encoding is specified
    encodingMapType::const_iterator it =  encodingMap.find(encoding);
    if (it != encodingMap.end())
      return it->second;

    // check, if a wildcard-rule is specified
    it = encodingMap.find("*");
    if (it != encodingMap.end())
      return it->second;

    // return 10 (accept), if encoding is identity, 0 otherwise
    return encoding == "identity" ? 10 : 0;
  }

}