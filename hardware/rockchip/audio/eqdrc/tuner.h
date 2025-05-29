/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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

/*
 * TODO: Add those to Android.mk to check build variant.
 * ifneq (,$(filter $(TARGET_BUILD_VARIANT),eng userdebug))
 * LOCAL_CFLAGS += EQDRC_TUNER_ENABLED
 * endif
 */

#ifndef EQDRC_TUNER_H_
#define EQDRC_TUNER_H_

#include "profile.h"

bool tuner_initialized(void);
bool tuner_synced(void);
int tuner_init(void);
int tuner_sync_profile(struct profile *profile, unsigned int sampling_rate, unsigned channels);

#endif /* EQDRC_TUNER_H_ */
