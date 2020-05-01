/*
 * Copyright (C) 2019 The Android Open Source Project
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
#ifndef EXYNOS_DISPLAY_MODULE_H
#define EXYNOS_DISPLAY_MODULE_H

#include "ExynosDisplay.h"
#include "ExynosPrimaryDisplay.h"
#include "ExynosLayer.h"
#include <displaycolor/displaycolor_gs101.h>

using namespace displaycolor;

class ExynosPrimaryDisplayModule : public ExynosPrimaryDisplay {
    public:
        ExynosPrimaryDisplayModule(uint32_t type, ExynosDevice *device);
        ~ExynosPrimaryDisplayModule();
        void usePreDefinedWindow(bool use);
        virtual int32_t validateWinConfigData();
        void doPreProcessing();
        virtual int32_t getColorModes(
                uint32_t* outNumModes,
                int32_t* outModes);
        virtual int32_t setColorMode(int32_t mode);
        virtual int32_t getRenderIntents(int32_t mode, uint32_t* outNumIntents,
                int32_t* outIntents);
        virtual int32_t setColorModeWithRenderIntent(int32_t mode,
                int32_t intent);
        virtual int deliverWinConfigData();
        virtual int32_t updateColorConversionInfo();

        class DisplaySceneInfo {
            public:
                bool colorSettingChanged = false;
                DisplayScene displayScene;

                /*
                 * Index of LayerColorData in displayScene.layer_data[]
                 * for each layer
                 * key: ExynosLayer*
                 * data: index in displayScene.layer_data[]
                 */
                std::map<ExynosLayer*, uint32_t> layerDataMappingInfo;
                std::map<ExynosLayer*, uint32_t> prev_layerDataMappingInfo;

                void reset() {
                    colorSettingChanged = false;
                    prev_layerDataMappingInfo = layerDataMappingInfo;
                    layerDataMappingInfo.clear();
                };

                void setColorMode(hwc::ColorMode mode) {
                    if (displayScene.color_mode != mode) {
                        colorSettingChanged = true;
                        displayScene.color_mode = mode;
                    }
                };

                void setRenderIntent(hwc::RenderIntent intent) {
                    if (displayScene.render_intent != intent) {
                        displayScene.render_intent = intent;
                        colorSettingChanged = true;
                    }
                };

                LayerColorData& getLayerColorDataInstance(uint32_t index);
                int32_t setLayerDataMappingInfo(ExynosLayer* layer, uint32_t index);
                void setLayerDataspace(LayerColorData& layerColorData,
                        hwc::Dataspace dataspace);
                void disableLayerHdrStaticMetadata(LayerColorData& layerColorData);
                void setLayerHdrStaticMetadata(LayerColorData& layerColorData,
                        const ExynosHdrStaticInfo& exynosHdrStaticInfo);
                int32_t setLayerColorData(LayerColorData& layerData,
                        ExynosLayer* layer);
                bool needDisplayColorSetting();
                void printDisplayScene();
                void printLayerColorData(const LayerColorData& layerData);
        };

        /* Call getDppForLayer() only if hasDppForLayer() is true */
        bool hasDppForLayer(ExynosLayer* layer);
        const IDisplayColorGS101::IDpp& getDppForLayer(ExynosLayer* layer);

        const IDisplayColorGS101::IDqe& getDqe()
        {
            return mDisplayColorInterface->Dqe();
        };

    private:
        int32_t setLayersColorData();
        std::unique_ptr<IDisplayColorGS101> mDisplayColorInterface;
        DisplaySceneInfo mDisplaySceneInfo;

};
#endif
