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

#include "ExynosDisplayDrmInterfaceModule.h"
#include "ExynosPrimaryDisplayModule.h"
#include <drm/samsung_drm.h>

template <typename T, typename M>
int32_t convertDqeMatrixDataToMatrix(
        const IDisplayColorGS101::MatrixData<T> &colorMatrix,
        M &mat, uint32_t dimension)
{
    if (colorMatrix.coeffs.size() != (dimension * dimension)) {
        HWC_LOGE(nullptr, "Invalid coeff size(%zu)",
                colorMatrix.coeffs.size());
        return -EINVAL;
    }
    for (uint32_t i = 0; i < (dimension * dimension); i++) {
        mat.coeffs[i] = colorMatrix.coeffs[i];
    }

    if (colorMatrix.offsets.size() != dimension) {
        HWC_LOGE(nullptr, "Invalid offset size(%zu)",
                colorMatrix.offsets.size());
        return -EINVAL;
    }
    for (uint32_t i = 0; i < dimension; i++) {
        mat.offsets[i] = colorMatrix.offsets[i];
    }
    return NO_ERROR;
}

/////////////////////////////////////////////////// ExynosDisplayDrmInterfaceModule //////////////////////////////////////////////////////////////////
ExynosDisplayDrmInterfaceModule::ExynosDisplayDrmInterfaceModule(ExynosDisplay *exynosDisplay)
: ExynosDisplayDrmInterface(exynosDisplay)
{
}

ExynosDisplayDrmInterfaceModule::~ExynosDisplayDrmInterfaceModule()
{
}

void ExynosDisplayDrmInterfaceModule::initDrmDevice(DrmDevice *drmDevice)
{
    ExynosDisplayDrmInterface::initDrmDevice(drmDevice);

    if (isPrimary() == false)
        return;

    mOldDqeBlobs.init(drmDevice);

    ExynosPrimaryDisplayModule* display =
        (ExynosPrimaryDisplayModule*)mExynosDisplay;
    size_t dppSize = display->getNumOfDpp();
    resizeOldDppBlobs(dppSize);
}

void ExynosDisplayDrmInterfaceModule::destroyOldBlobs(
        std::vector<uint32_t> &oldBlobs)
{
    for (auto &blob : oldBlobs) {
        mDrmDevice->DestroyPropertyBlob(blob);
    }
    oldBlobs.clear();
}

int32_t ExynosDisplayDrmInterfaceModule::createCgcBlobFromIDqe(
        const IDisplayColorGS101::IDqe &dqe, uint32_t &blobId)
{
    struct cgc_lut cgc;
    const IDisplayColorGS101::IDqe::CgcData &cgcData = dqe.Cgc();
    if ((cgcData.r_values.size() != DRM_SAMSUNG_CGC_LUT_REG_CNT) ||
        (cgcData.g_values.size() != DRM_SAMSUNG_CGC_LUT_REG_CNT) ||
        (cgcData.b_values.size() != DRM_SAMSUNG_CGC_LUT_REG_CNT)) {
        ALOGE("CGC data size is not same (r: %zu, g: %zu: b: %zu)",
                cgcData.r_values.size(), cgcData.g_values.size(),
                cgcData.b_values.size());
        return -EINVAL;
    }

    for (uint32_t i = 0; i < DRM_SAMSUNG_CGC_LUT_REG_CNT; i++) {
        cgc.r_values[i] = cgcData.r_values[i];
        cgc.g_values[i] = cgcData.g_values[i];
        cgc.b_values[i] = cgcData.b_values[i];
    }
    int ret = mDrmDevice->CreatePropertyBlob(&cgc, sizeof(cgc_lut), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create cgc blob %d", ret);
        return ret;
    }
    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createDegammaLutBlobFromIDqe(
        const IDisplayColorGS101::IDqe &dqe, uint32_t &blobId)
{
    int ret = 0;
    uint64_t lut_size = 0;
    std::tie(ret, lut_size) = mDrmCrtc->degamma_lut_size_property().value();
    if (ret < 0) {
         HWC_LOGE(mExynosDisplay, "%s: there is no degamma_lut_size (ret = %d)",
                 __func__, ret);
         return ret;
    }
    if (lut_size != IDisplayColorGS101::IDqe::DegammaLutData::kLutLen) {
        HWC_LOGE(mExynosDisplay, "%s: invalid lut size (%" PRId64 ")",
                __func__, lut_size);
        return -EINVAL;
    }

    struct drm_color_lut color_lut[IDisplayColorGS101::IDqe::DegammaLutData::kLutLen];
    for (uint32_t i = 0; i < lut_size; i++) {
        color_lut[i].red = dqe.DegammaLut().values[i];
    }
    ret = mDrmDevice->CreatePropertyBlob(color_lut, sizeof(color_lut), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create degamma lut blob %d", ret);
        return ret;
    }
    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createRegammaLutBlobFromIDqe(
        const IDisplayColorGS101::IDqe &dqe, uint32_t &blobId)
{
    int ret = 0;
    uint64_t lut_size = 0;
    std::tie(ret, lut_size) = mDrmCrtc->gamma_lut_size_property().value();
    if (ret < 0) {
         HWC_LOGE(mExynosDisplay, "%s: there is no gamma_lut_size (ret = %d)",
                 __func__, ret);
         return ret;
    }
    if (lut_size != IDisplayColorGS101::IDqe::DegammaLutData::kLutLen) {
        HWC_LOGE(mExynosDisplay, "%s: invalid lut size (%" PRId64 ")",
                __func__, lut_size);
        return -EINVAL;
    }

    struct drm_color_lut color_lut[IDisplayColorGS101::IDqe::DegammaLutData::kLutLen];
    for (uint32_t i = 0; i < lut_size; i++) {
        color_lut[i].red = dqe.RegammaLut().r_values[i];
        color_lut[i].green = dqe.RegammaLut().g_values[i];
        color_lut[i].blue = dqe.RegammaLut().b_values[i];
    }
    ret = mDrmDevice->CreatePropertyBlob(color_lut, sizeof(color_lut), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create gamma lut blob %d", ret);
        return ret;
    }
    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createGammaMatBlobFromIDqe(
        const IDisplayColorGS101::IDqe &dqe, uint32_t &blobId)
{
    int ret = 0;
    struct exynos_matrix gamma_matrix;
    if ((ret = convertDqeMatrixDataToMatrix(
                    dqe.GammaMatrix(), gamma_matrix, DRM_SAMSUNG_MATRIX_DIMENS)) != NO_ERROR)
    {
        HWC_LOGE(mExynosDisplay, "Failed to convert gamma matrix");
        return ret;
    }
    ret = mDrmDevice->CreatePropertyBlob(&gamma_matrix, sizeof(gamma_matrix), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create gamma matrix blob %d", ret);
        return ret;
    }

    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createLinearMatBlobFromIDqe(
        const IDisplayColorGS101::IDqe &dqe, uint32_t &blobId)
{
    int ret = 0;
    struct exynos_matrix linear_matrix;
    if ((ret = convertDqeMatrixDataToMatrix(
                    dqe.LinearMatrix(), linear_matrix, DRM_SAMSUNG_MATRIX_DIMENS)) != NO_ERROR)
    {
        HWC_LOGE(mExynosDisplay, "Failed to convert linear matrix");
        return ret;
    }
    ret = mDrmDevice->CreatePropertyBlob(&linear_matrix, sizeof(linear_matrix), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create linear matrix blob %d", ret);
        return ret;
    }

    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createEotfBlobFromIDpp(
        const IDisplayColorGS101::IDpp &dpp, uint32_t &blobId)
{
    struct hdr_eotf_lut eotf_lut;
    if ((dpp.EotfLut().posx.size() != DRM_SAMSUNG_HDR_EOTF_LUT_LEN) ||
        (dpp.EotfLut().posy.size() != DRM_SAMSUNG_HDR_EOTF_LUT_LEN)) {
        HWC_LOGE(mExynosDisplay, "%s: eotf pos size (%zu, %zu)",
                __func__, dpp.EotfLut().posx.size(), dpp.EotfLut().posy.size());
        return -EINVAL;
    }
    for (uint32_t i = 0; i < DRM_SAMSUNG_HDR_EOTF_LUT_LEN; i++) {
        eotf_lut.posx[i] = dpp.EotfLut().posx[i];
        eotf_lut.posy[i] = dpp.EotfLut().posy[i];
    }
    int ret = mDrmDevice->CreatePropertyBlob(&eotf_lut, sizeof(eotf_lut), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create eotf lut blob %d", ret);
        return ret;
    }
    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createGmBlobFromIDpp(
        const IDisplayColorGS101::IDpp &dpp, uint32_t &blobId)
{
    int ret = 0;
    struct hdr_gm_data gm_matrix;
    if ((ret = convertDqeMatrixDataToMatrix(
                    dpp.Gm(), gm_matrix, DRM_SAMSUNG_HDR_GM_DIMENS)) != NO_ERROR)
    {
        HWC_LOGE(mExynosDisplay, "Failed to convert gm matrix");
        return ret;
    }
    ret = mDrmDevice->CreatePropertyBlob(&gm_matrix, sizeof(gm_matrix), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create gm matrix blob %d", ret);
        return ret;
    }
    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::createDtmBlobFromIDpp(
        const IDisplayColorGS101::IDpp &dpp, uint32_t &blobId)
{
    struct hdr_tm_data tm_data;
    if ((dpp.Dtm().posx.size() != DRM_SAMSUNG_HDR_TM_LUT_LEN) ||
        (dpp.Dtm().posy.size() != DRM_SAMSUNG_HDR_TM_LUT_LEN)) {
        HWC_LOGE(mExynosDisplay, "%s: dtm pos size (%zu, %zu)",
                __func__, dpp.Dtm().posx.size(), dpp.Dtm().posy.size());
        return -EINVAL;
    }

    for (uint32_t i = 0; i < DRM_SAMSUNG_HDR_TM_LUT_LEN; i++) {
        tm_data.posx[i] = dpp.Dtm().posx[i];
        tm_data.posy[i] = dpp.Dtm().posy[i];
    }

    tm_data.coeff_r = dpp.Dtm().coeff_r;
    tm_data.coeff_g = dpp.Dtm().coeff_g;
    tm_data.coeff_b = dpp.Dtm().coeff_b;
    tm_data.rng_x_min = dpp.Dtm().rng_x_min;
    tm_data.rng_x_max = dpp.Dtm().rng_x_max;
    tm_data.rng_y_min = dpp.Dtm().rng_y_min;
    tm_data.rng_y_max = dpp.Dtm().rng_y_max;

    int ret = mDrmDevice->CreatePropertyBlob(&tm_data, sizeof(tm_data), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create tm_data blob %d", ret);
        return ret;
    }

    return NO_ERROR;
}
int32_t ExynosDisplayDrmInterfaceModule::createOetfBlobFromIDpp(
        const IDisplayColorGS101::IDpp &dpp, uint32_t &blobId)
{
    struct hdr_oetf_lut oetf_lut;
    if ((dpp.OetfLut().posx.size() != DRM_SAMSUNG_HDR_OETF_LUT_LEN) ||
        (dpp.OetfLut().posy.size() != DRM_SAMSUNG_HDR_OETF_LUT_LEN)) {
        HWC_LOGE(mExynosDisplay, "%s: oetf pos size (%zu, %zu)",
                __func__, dpp.OetfLut().posx.size(), dpp.OetfLut().posy.size());
        return -EINVAL;
    }
    for (uint32_t i = 0; i < DRM_SAMSUNG_HDR_OETF_LUT_LEN; i++) {
        oetf_lut.posx[i] = dpp.OetfLut().posx[i];
        oetf_lut.posy[i] = dpp.OetfLut().posy[i];
    }
    int ret = mDrmDevice->CreatePropertyBlob(&oetf_lut, sizeof(oetf_lut), &blobId);
    if (ret) {
        HWC_LOGE(mExynosDisplay, "Failed to create oetf lut blob %d", ret);
        return ret;
    }
    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::setDisplayColorBlob(
        const DrmProperty &prop,
        const uint32_t type,
        const IDisplayColorGeneric::DisplayStage &stage,
        const IDisplayColorGS101::IDqe &dqe,
        ExynosDisplayDrmInterface::DrmModeAtomicReq &drmReq)
{
    if (!prop.id())
        return NO_ERROR;

    int32_t ret = 0;
    uint32_t blobId = 0;
    if (stage.enable) {
        if (stage.dirty) {
            switch (type) {
                case DqeBlobs::CGC:
                    ret = createCgcBlobFromIDqe(dqe, blobId);
                    break;
                case DqeBlobs::DEGAMMA_LUT:
                    ret = createDegammaLutBlobFromIDqe(dqe, blobId);
                    break;
                case DqeBlobs::REGAMMA_LUT:
                    ret = createRegammaLutBlobFromIDqe(dqe, blobId);
                    break;
                case DqeBlobs::GAMMA_MAT:
                    ret = createGammaMatBlobFromIDqe(dqe, blobId);
                    break;
                case DqeBlobs::LINEAR_MAT:
                    ret = createLinearMatBlobFromIDqe(dqe, blobId);
                    break;
                default:
                    ret = -EINVAL;
            }
            if (ret != NO_ERROR) {
                HWC_LOGE(mExynosDisplay, "%s: create blob fail", __func__);
                return ret;
            }
            mOldDqeBlobs.addBlob(type, blobId);
        } else {
            blobId = mOldDqeBlobs.getBlob(type);
        }
    }
    if ((ret = drmReq.atomicAddProperty(mDrmCrtc->id(), prop, blobId)) < 0) {
        HWC_LOGE(mExynosDisplay, "%s: Fail to set property",
                __func__);
        return ret;
    }
    return ret;
}
int32_t ExynosDisplayDrmInterfaceModule::setDisplayColorSetting(
        ExynosDisplayDrmInterface::DrmModeAtomicReq &drmReq)
{
    if ((mColorSettingChanged == false) ||
        (isPrimary() == false))
        return NO_ERROR;

    ExynosPrimaryDisplayModule* display =
        (ExynosPrimaryDisplayModule*)mExynosDisplay;

    int ret = NO_ERROR;
    const IDisplayColorGS101::IDqe &dqe = display->getDqe();

    if ((ret = setDisplayColorBlob(mDrmCrtc->cgc_lut_property(),
                static_cast<uint32_t>(DqeBlobs::CGC),
                dqe.Cgc(), dqe, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: set Cgc blob fail", __func__);
        return ret;
    }
    if ((ret = setDisplayColorBlob(mDrmCrtc->degamma_lut_property(),
                static_cast<uint32_t>(DqeBlobs::DEGAMMA_LUT),
                dqe.DegammaLut(), dqe, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: set DegammaLut blob fail", __func__);
        return ret;
    }
    if ((ret = setDisplayColorBlob(mDrmCrtc->gamma_lut_property(),
                static_cast<uint32_t>(DqeBlobs::REGAMMA_LUT),
                dqe.RegammaLut(), dqe, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: set RegammaLut blob fail", __func__);
        return ret;
    }
    if ((ret = setDisplayColorBlob(mDrmCrtc->gamma_matrix_property(),
                static_cast<uint32_t>(DqeBlobs::GAMMA_MAT),
                dqe.GammaMatrix(), dqe, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: set GammaMatrix blob fail", __func__);
        return ret;
    }
    if ((ret = setDisplayColorBlob(mDrmCrtc->linear_matrix_property(),
                static_cast<uint32_t>(DqeBlobs::LINEAR_MAT),
                dqe.LinearMatrix(), dqe, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: set LinearMatrix blob fail", __func__);
        return ret;
    }

    return NO_ERROR;
}

int32_t ExynosDisplayDrmInterfaceModule::setPlaneColorBlob(
        const std::unique_ptr<DrmPlane> &plane,
        const DrmProperty &prop,
        const uint32_t type,
        const IDisplayColorGeneric::DisplayStage &stage,
        const IDisplayColorGS101::IDpp &dpp,
        const uint32_t dppIndex,
        ExynosDisplayDrmInterface::DrmModeAtomicReq &drmReq)
{
    if (!prop.id())
        return NO_ERROR;

    if (dppIndex >= mOldDppBlobs.size()) {
        HWC_LOGE(mExynosDisplay, "%s: invalid dpp index(%d)", __func__, dppIndex);
        return -EINVAL;
    }
    DppBlobs &oldDppBlobs = mOldDppBlobs[dppIndex];

    int32_t ret = 0;
    uint32_t blobId = 0;

    if (stage.enable) {
        if (stage.dirty) {
            switch (type) {
                case DppBlobs::EOTF:
                    ret = createEotfBlobFromIDpp(dpp, blobId);
                    break;
                case DppBlobs::GM:
                    ret = createGmBlobFromIDpp(dpp, blobId);
                    break;
                case DppBlobs::DTM:
                    ret = createDtmBlobFromIDpp(dpp, blobId);
                    break;
                case DppBlobs::OETF:
                    ret = createOetfBlobFromIDpp(dpp, blobId);
                    break;
                default:
                    ret = -EINVAL;
            }
            if (ret != NO_ERROR) {
                HWC_LOGE(mExynosDisplay, "%s: create blob fail", __func__);
                return ret;
            }
            oldDppBlobs.addBlob(type, blobId);
        } else {
            blobId = oldDppBlobs.getBlob(type);
        }
    }
    if ((ret = drmReq.atomicAddProperty(plane->id(), prop, blobId)) < 0) {
        HWC_LOGE(mExynosDisplay, "%s: Fail to set property",
                __func__);
        return ret;
    }

    return ret;
}

int32_t ExynosDisplayDrmInterfaceModule::setPlaneColorSetting(
        ExynosDisplayDrmInterface::DrmModeAtomicReq &drmReq,
        const std::unique_ptr<DrmPlane> &plane,
        const exynos_win_config_data &config)
{
    if ((mColorSettingChanged == false) ||
        (isPrimary() == false))
        return NO_ERROR;

    if ((config.assignedMPP == nullptr) ||
        (config.assignedMPP->mAssignedSources.size() == 0)) {
        HWC_LOGE(mExynosDisplay, "%s:: config's mpp source size is invalid",
                __func__);
        return -EINVAL;
    }
    ExynosMPPSource* mppSource = config.assignedMPP->mAssignedSources[0];

    /*
     * Color conversion of Client and Exynos composition buffer
     * is already addressed by GLES or G2D
     */
    if (mppSource->mSourceType == MPP_SOURCE_COMPOSITION_TARGET)
        return NO_ERROR;

    if (mppSource->mSourceType != MPP_SOURCE_LAYER) {
        HWC_LOGE(mExynosDisplay, "%s:: invalid mpp source type (%d)",
                __func__, mppSource->mSourceType);
        return -EINVAL;
    }

    ExynosLayer* layer = (ExynosLayer*)mppSource;

    /* color conversion was already handled by m2mMPP */
    if ((layer->mM2mMPP != nullptr) &&
        (layer->mSrcImg.dataSpace != layer->mMidImg.dataSpace)) {
        return NO_ERROR;
    }

    ExynosPrimaryDisplayModule* display =
        (ExynosPrimaryDisplayModule*)mExynosDisplay;

    size_t dppSize = display->getNumOfDpp();
    resizeOldDppBlobs(dppSize);

    if (display->hasDppForLayer(layer) == false) {
        HWC_LOGE(mExynosDisplay,
                "%s: layer need color conversion but there is no IDpp",
                __func__);
        return -EINVAL;
    }

    const IDisplayColorGS101::IDpp &dpp = display->getDppForLayer(layer);
    const uint32_t dppIndex = static_cast<uint32_t>(display->getDppIndexForLayer(layer));

    int ret = 0;
    if ((ret = setPlaneColorBlob(plane, plane->eotf_lut_property(),
                static_cast<uint32_t>(DppBlobs::EOTF),
                dpp.EotfLut(), dpp, dppIndex, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: dpp[%d] set oetf blob fail",
                __func__, dppIndex);
        return ret;
    }
    if ((ret = setPlaneColorBlob(plane, plane->gammut_matrix_property(),
                static_cast<uint32_t>(DppBlobs::GM),
                dpp.Gm(), dpp, dppIndex, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: dpp[%d] set GM blob fail",
                __func__, dppIndex);
        return ret;
    }
    if ((ret = setPlaneColorBlob(plane, plane->tone_mapping_property(),
                static_cast<uint32_t>(DppBlobs::DTM),
                dpp.Dtm(), dpp, dppIndex, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: dpp[%d] set DTM blob fail",
                __func__, dppIndex);
        return ret;
    }
    if ((ret = setPlaneColorBlob(plane, plane->oetf_lut_property(),
                static_cast<uint32_t>(DppBlobs::OETF),
                dpp.OetfLut(), dpp, dppIndex, drmReq) != NO_ERROR)) {
        HWC_LOGE(mExynosDisplay, "%s: dpp[%d] set OETF blob fail",
                __func__, dppIndex);
        return ret;
    }

    return 0;
}

ExynosDisplayDrmInterfaceModule::SaveBlob::~SaveBlob()
{
    for (auto &it: blobs) {
        mDrmDevice->DestroyPropertyBlob(it);
    }
    blobs.clear();
}

void ExynosDisplayDrmInterfaceModule::SaveBlob::addBlob(
        uint32_t type, uint32_t blob)
{
    if (type >= blobs.size()) {
        ALOGE("Invalid dqe blop type: %d", type);
        return;
    }
    if (blobs[type] > 0)
        mDrmDevice->DestroyPropertyBlob(blobs[type]);

    blobs[type] = blob;
}

uint32_t ExynosDisplayDrmInterfaceModule::SaveBlob::getBlob(uint32_t type)
{
    if (type >= blobs.size()) {
        ALOGE("Invalid dqe blop type: %d", type);
        return 0;
    }
    return blobs[type];
}

//////////////////////////////////////////////////// ExynosPrimaryDisplayDrmInterfaceModule //////////////////////////////////////////////////////////////////
ExynosPrimaryDisplayDrmInterfaceModule::ExynosPrimaryDisplayDrmInterfaceModule(ExynosDisplay *exynosDisplay)
: ExynosDisplayDrmInterfaceModule(exynosDisplay)
{
}

ExynosPrimaryDisplayDrmInterfaceModule::~ExynosPrimaryDisplayDrmInterfaceModule()
{
}

//////////////////////////////////////////////////// ExynosExternalDisplayDrmInterfaceModule //////////////////////////////////////////////////////////////////
ExynosExternalDisplayDrmInterfaceModule::ExynosExternalDisplayDrmInterfaceModule(ExynosDisplay *exynosDisplay)
: ExynosDisplayDrmInterfaceModule(exynosDisplay)
{
}

ExynosExternalDisplayDrmInterfaceModule::~ExynosExternalDisplayDrmInterfaceModule()
{
}
