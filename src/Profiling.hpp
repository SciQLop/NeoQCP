/*------------------------------------------------------------------------------
-- This file is a part of the NeoQCP project.
-- Copyright (C) 2025, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once

#ifdef TRACY_ENABLE

#include <tracy/Tracy.hpp>
#define PROFILE_HERE ZoneScoped
#define PROFILE_HERE_N(name) ZoneScopedN(name)
#define PROFILE_HERE_NC(name, color) ZoneScopedNC(name, color)
#define PROFILE_PASS_VALUE(value) ZoneValue(value)

#else

#define PROFILE_HERE
#define PROFILE_HERE_N(name)
#define PROFILE_HERE_NC(name, color)
#define PROFILE_PASS_VALUE(value)

#endif
