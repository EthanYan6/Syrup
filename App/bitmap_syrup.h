/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef BITMAP_SYRUP_H
#define BITMAP_SYRUP_H

#include <stdint.h>

#define BITMAP_SYRUP_WIDTH  48
#define BITMAP_SYRUP_HEIGHT 32
#define BITMAP_SYRUP_PAGES  (BITMAP_SYRUP_HEIGHT / 8)

extern const uint8_t BITMAP_Syrup[BITMAP_SYRUP_PAGES * BITMAP_SYRUP_WIDTH];

#endif
