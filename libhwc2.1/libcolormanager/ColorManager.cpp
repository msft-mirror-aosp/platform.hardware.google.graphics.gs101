/*
 * Copyright (C) 2023 The Android Open Source Project
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
#include "ColorManager.h"

#include "BrightnessController.h"
#include "ExynosDisplay.h"
#include "ExynosHWCDebug.h"

namespace gs101 {

#define CLR_LOGD(msg, ...) \
    ALOGD("[%s] %s: " msg, mExynosDisplay->mDisplayName.c_str(), __func__, ##__VA_ARGS__);

int32_t ColorManager::getColorModes(uint32_t* outNumModes, int32_t* outModes) {
    GsInterfaceType* displayColorInterface = getDisplayColorInterface();
    const DisplayType display = mExynosDisplay->getDcDisplayType();
    const ColorModesMap colorModeMap = displayColorInterface == nullptr
            ? ColorModesMap()
            : displayColorInterface->ColorModesAndRenderIntents(display);
    CLR_LOGD("size(%zu)", colorModeMap.size());
    if (outNumModes == nullptr) {
        DISPLAY_DRM_LOGE("%s: outNumModes is null", __func__);
        return HWC2_ERROR_BAD_PARAMETER;
    }
    if (outModes == nullptr) {
        *outNumModes = colorModeMap.size();
        return HWC2_ERROR_NONE;
    }
    if (*outNumModes != colorModeMap.size()) {
        DISPLAY_DRM_LOGE("%s: Invalid color mode size(%d), It should be(%zu)", __func__,
                         *outNumModes, colorModeMap.size());
        return HWC2_ERROR_BAD_PARAMETER;
    }

    uint32_t index = 0;
    for (const auto& it : colorModeMap) {
        outModes[index] = static_cast<int32_t>(it.first);
        CLR_LOGD("\tmode[%d] %d", index, outModes[index]);
        index++;
    }

    return HWC2_ERROR_NONE;
}

int32_t ColorManager::setColorMode(int32_t mode) {
    CLR_LOGD("mode(%d)", mode);
    GsInterfaceType* displayColorInterface = getDisplayColorInterface();
    const DisplayType display = mExynosDisplay->getDcDisplayType();
    const ColorModesMap colorModeMap = displayColorInterface == nullptr
            ? ColorModesMap()
            : displayColorInterface->ColorModesAndRenderIntents(display);
    hwc::ColorMode colorMode = static_cast<hwc::ColorMode>(mode);
    const auto it = colorModeMap.find(colorMode);
    if (it == colorModeMap.end()) {
        DISPLAY_DRM_LOGE("%s: Invalid color mode(%d)", __func__, mode);
        return HWC2_ERROR_BAD_PARAMETER;
    }
    mDisplaySceneInfo.setColorMode(colorMode);

    if (mExynosDisplay->mColorMode != mode)
        mExynosDisplay->setGeometryChanged(GEOMETRY_DISPLAY_COLOR_MODE_CHANGED);
    mExynosDisplay->mColorMode = (android_color_mode_t)mode;

    return HWC2_ERROR_NONE;
}

int32_t ColorManager::getRenderIntents(int32_t mode, uint32_t* outNumIntents, int32_t* outIntents) {
    GsInterfaceType* displayColorInterface = getDisplayColorInterface();
    const DisplayType display = mExynosDisplay->getDcDisplayType();
    const ColorModesMap colorModeMap = displayColorInterface == nullptr
            ? ColorModesMap()
            : displayColorInterface->ColorModesAndRenderIntents(display);
    CLR_LOGD("size(%zu)", colorModeMap.size());
    hwc::ColorMode colorMode = static_cast<hwc::ColorMode>(mode);
    const auto it = colorModeMap.find(colorMode);
    if (it == colorModeMap.end()) {
        DISPLAY_DRM_LOGE("%s: Invalid color mode(%d)", __func__, mode);
        return HWC2_ERROR_BAD_PARAMETER;
    }
    auto& renderIntents = it->second;
    if (outIntents == NULL) {
        *outNumIntents = renderIntents.size();
        CLR_LOGD("\tintent num(%zu)", renderIntents.size());
        return HWC2_ERROR_NONE;
    }
    if (*outNumIntents != renderIntents.size()) {
        DISPLAY_DRM_LOGE("%s: Invalid intent size(%d), It should be(%zu)", __func__, *outNumIntents,
                         renderIntents.size());
        return HWC2_ERROR_BAD_PARAMETER;
    }

    for (uint32_t i = 0; i < renderIntents.size(); i++) {
        outIntents[i] = static_cast<uint32_t>(renderIntents[i]);
        CLR_LOGD("\tintent[%d] %d", i, outIntents[i]);
    }

    return HWC2_ERROR_NONE;
}

int32_t ColorManager::setColorModeWithRenderIntent(int32_t mode, int32_t intent) {
    GsInterfaceType* displayColorInterface = getDisplayColorInterface();
    const DisplayType display = mExynosDisplay->getDcDisplayType();
    const ColorModesMap colorModeMap = displayColorInterface == nullptr
            ? ColorModesMap()
            : displayColorInterface->ColorModesAndRenderIntents(display);
    hwc::ColorMode colorMode = static_cast<hwc::ColorMode>(mode);
    hwc::RenderIntent renderIntent = static_cast<hwc::RenderIntent>(intent);

    const auto mode_it = colorModeMap.find(colorMode);
    if (mode_it == colorModeMap.end()) {
        DISPLAY_DRM_LOGE("%s: Invalid color mode(%d)", __func__, mode);
        return HWC2_ERROR_BAD_PARAMETER;
    }

    auto& renderIntents = mode_it->second;
    auto intent_it = std::find(renderIntents.begin(), renderIntents.end(), renderIntent);
    if (intent_it == renderIntents.end()) {
        DISPLAY_DRM_LOGE("%s: Invalid render intent(%d)", __func__, intent);
        return HWC2_ERROR_BAD_PARAMETER;
    }

    mDisplaySceneInfo.setColorMode(colorMode);
    mDisplaySceneInfo.setRenderIntent(renderIntent);

    if (mExynosDisplay->mColorMode != mode) {
        CLR_LOGD("mode(%d), intent(%d)", mode, intent);
        mExynosDisplay->setGeometryChanged(GEOMETRY_DISPLAY_COLOR_MODE_CHANGED);
    }
    mExynosDisplay->mColorMode = (android_color_mode_t)mode;

    if (mExynosDisplay->mBrightnessController)
        mExynosDisplay->mBrightnessController->updateColorRenderIntent(intent);

    return HWC2_ERROR_NONE;
}

int32_t ColorManager::setColorTransform(const float* matrix, int32_t hint) {
    if ((hint < HAL_COLOR_TRANSFORM_IDENTITY) || (hint > HAL_COLOR_TRANSFORM_CORRECT_TRITANOPIA))
        return HWC2_ERROR_BAD_PARAMETER;
    if (mExynosDisplay->mColorTransformHint != hint) {
        ALOGI("[%s] %s:: %d -> %d", mExynosDisplay->mDisplayName.c_str(), __func__,
              mExynosDisplay->mColorTransformHint, hint);
        mExynosDisplay->setGeometryChanged(GEOMETRY_DISPLAY_COLOR_TRANSFORM_CHANGED);
    }
    mExynosDisplay->mColorTransformHint = hint;
#ifdef HWC_SUPPORT_COLOR_TRANSFORM
    mDisplaySceneInfo.setColorTransform(matrix);
#endif
    return HWC2_ERROR_NONE;
}

bool ColorManager::hasDppForLayer(ExynosMPPSource* layer) {
    GsInterfaceType* displayColorInterface = getDisplayColorInterface();
    if (displayColorInterface == nullptr) {
        return false;
    }

    if (getDisplaySceneInfo().layerDataMappingInfo.count(layer) == 0) return false;

    uint32_t index = getDisplaySceneInfo().layerDataMappingInfo[layer].dppIdx;
    const DisplayType display = mExynosDisplay->getDcDisplayType();
    auto size = displayColorInterface->GetPipelineData(display)->Dpp().size();
    if (index >= size) {
        DISPLAY_DRM_LOGE("%s: invalid dpp index(%d) dpp size(%zu)", __func__, index, size);
        return false;
    }

    return true;
}

const ColorManager::GsInterfaceType::IDpp& ColorManager::getDppForLayer(ExynosMPPSource* layer) {
    uint32_t index = getDisplaySceneInfo().layerDataMappingInfo[layer].dppIdx;
    GsInterfaceType* displayColorInterface = getDisplayColorInterface();
    const DisplayType display = mExynosDisplay->getDcDisplayType();
    return displayColorInterface->GetPipelineData(display)->Dpp()[index].get();
}

int32_t ColorManager::getDppIndexForLayer(ExynosMPPSource* layer) {
    if (getDisplaySceneInfo().layerDataMappingInfo.count(layer) == 0) return -1;
    uint32_t index = getDisplaySceneInfo().layerDataMappingInfo[layer].dppIdx;

    return static_cast<int32_t>(index);
}

} // namespace gs101
