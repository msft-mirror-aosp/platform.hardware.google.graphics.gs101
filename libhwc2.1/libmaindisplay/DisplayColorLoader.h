/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef DISPLAY_COLOR_LOADER_H
#define DISPLAY_COLOR_LOADER_H

#include <dlfcn.h>
#include <gs101/displaycolor/displaycolor_gs101.h>
#include <log/log.h>
#include <string>

namespace gs101 {

class DisplayColorLoader {
    public:
      DisplayColorLoader(const char *lib_name) {
          lib_handle = dlopen(lib_name, RTLD_LAZY);

          if (lib_handle != nullptr) {
              get_display_color_gs101 = (decltype(get_display_color_gs101))
                                    dlsym(lib_handle, "GetDisplayColorGS101");

              if (get_display_color_gs101 == nullptr) {
                  ALOGE("%s: failed to get GetDisplayColorGS101\n", __func__);
              }
          } else {
              ALOGE("%s: failed to load library %s\n", __func__, lib_name);
              get_display_color_gs101 = nullptr;
          }
      }

      displaycolor::IDisplayColorGS101* GetDisplayColorGS101(size_t display_num) {
          if (get_display_color_gs101 != nullptr) {
              return get_display_color_gs101(display_num);
          }

          return nullptr;
      }

      ~DisplayColorLoader() {
          if (lib_handle != nullptr) {
              dlclose(lib_handle);
          }
      }

    private:
      void *lib_handle;
      displaycolor::IDisplayColorGS101* (*get_display_color_gs101)(size_t);
};

}  // namespace gs101

#endif //DISPLAY_COLOR_LOADER_H
