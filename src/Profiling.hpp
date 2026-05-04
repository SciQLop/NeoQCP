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

// Macros are guarded with #ifndef so that consumers (e.g. SciQLopPlots) can
// provide their own definitions without colliding when both headers end up in
// the same translation unit via transitive includes.
#ifdef TRACY_ENABLE

#include <tracy/Tracy.hpp>

#ifndef PROFILE_FRAME_MARK
#  define PROFILE_FRAME_MARK FrameMark
#endif
#ifndef PROFILE_HERE
#  define PROFILE_HERE ZoneScoped
#endif
#ifndef PROFILE_HERE_N
#  define PROFILE_HERE_N(name) ZoneScopedN(name)
#endif
#ifndef PROFILE_HERE_NC
#  define PROFILE_HERE_NC(name, color) ZoneScopedNC(name, color)
#endif
#ifndef PROFILE_PASS_VALUE
#  define PROFILE_PASS_VALUE(value) ZoneValue(value)
#endif
#ifndef PROFILE_PASS_TXT
#  define PROFILE_PASS_TXT(txt, size) ZoneText(txt, size)
#endif
#ifndef PROFILE_PASS_VALUE_N
#  define PROFILE_PASS_VALUE_N(name, value) ZoneValue(value)
#endif

#else

#ifndef PROFILE_FRAME_MARK
#  define PROFILE_FRAME_MARK
#endif
#ifndef PROFILE_HERE
#  define PROFILE_HERE
#endif
#ifndef PROFILE_HERE_N
#  define PROFILE_HERE_N(name)
#endif
#ifndef PROFILE_HERE_NC
#  define PROFILE_HERE_NC(name, color)
#endif
#ifndef PROFILE_PASS_VALUE
#  define PROFILE_PASS_VALUE(value)
#endif
#ifndef PROFILE_PASS_VALUE_N
#  define PROFILE_PASS_VALUE_N(name, value)
#endif
#ifndef PROFILE_PASS_TXT
#  define PROFILE_PASS_TXT(txt, size)
#endif

#endif
