/*
 * Copyright (C) 2023 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ANDROID_HWJPEG_VERSION_H_
#define _ANDROID_HWJPEG_VERSION_H_

#define VERSION_STR_HELPER(x) #x
#define VERSION_STR(x) VERSION_STR_HELPER(x)

/* Libhwjpeg Component Verison */
#define MAJOR_VERSION       1
#define MINOR_VERSION       5
#define REVISION_VERSION    5

#define HWJPEG_FULL_VERSION \
    VERSION_STR(MAJOR_VERSION) "." \
    VERSION_STR(MINOR_VERSION) "." \
    VERSION_STR(REVISION_VERSION) ""

#endif /* _ANDROID_HWJPEG_VERSION_H_ */

