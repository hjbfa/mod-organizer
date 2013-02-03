/*
Mod Organizer shared UI functionality

Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef IMODINTERFACE_H
#define IMODINTERFACE_H


#include "versioninfo.h"


class IModInterface
{
public:

  virtual ~IModInterface() {}

  /**
   * @return name of the mod
   */
  virtual QString name() const = 0;

  /**
   * @return absolute path to the mod to be used in file system operations
   */
  virtual QString absolutePath() const = 0;

  /**
   * @brief set/change the version of this mod
   * @param version new version of the mod
   */
  virtual void setVersion(const VersionInfo &version) = 0;

  /**
   * @brief sets the mod id on nexus for this mod
   * @param the new id to set
   */
  virtual void setNexusID(int nexusID) = 0;

};


#endif // IMODINTERFACE_H
