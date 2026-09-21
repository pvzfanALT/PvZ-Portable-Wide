/*
 * Copyright (C) 2026 Zhou Qiankang <wszqkzqk@qq.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This file is part of PvZ-Portable.
 *
 * PvZ-Portable is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PvZ-Portable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PvZ-Portable. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __VITAPLATFORM_H__
#define __VITAPLATFORM_H__

namespace Sexy
{

// The Vita's panel is a fixed 960x544; SDL reports this as the only display
// mode unless VITA_RESOLUTION asks the PVR driver to upscale for a PSTV.
constexpr int VITA_SCREEN_WIDTH = 960;
constexpr int VITA_SCREEN_HEIGHT = 544;

} // namespace Sexy

#endif // __VITAPLATFORM_H__
