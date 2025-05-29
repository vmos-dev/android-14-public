/*
 * Copyright 2024 Rockchip Electronics Co. LTD.
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

#define LOG_TAG "preproc-profile"
// #define LOG_NDEBUG 0

#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <rkaudio_aec_bf.h>
#include "profile.h"
#include "preprocess.h"

#define PROFILE_PATH "/vendor/etc/rkaudio_effect_preprocess.xml"

#define MAX_CHN 16

#define SAFE_FREE(p) \
    if (p) { \
        free(p); \
        p = NULL; \
    }

static int prop_get_short(xmlNodePtr node, short *val)
{
    xmlNodePtr child = node->children;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    *val = atoi((const char *)child->content);

    return 0;
}

static int prop_get_int(xmlNodePtr node, int *val)
{
    xmlNodePtr child = node->children;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    *val = atoi((const char *)child->content);

    return 0;
}

static int prop_get_float(xmlNodePtr node, float *val)
{
    xmlNodePtr child = node->children;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    *val = atof((const char *)child->content);

    return 0;
}

static int prop_get_channel_array(xmlNodePtr node, short **array)
{
    xmlNodePtr child = node->children;
    short *tmp;
    char *str;
    char *token;
    int channels = 1;
    int i;
    int ret = 0;

    if (!child || child->type != XML_TEXT_NODE || !child->content) {
        ret = -EINVAL;
        goto out;
    }

    str = strdup((const char *)child->content);
    if (!str) {
        ret = -ENOMEM;
        goto out;
    }

    for (i = 0; str[i]; i++) {
        if (str[i] == ',')
            channels++;
    }

    if (channels > MAX_CHN) {
        ALOGE("%s: channel %d exceeds the maximum %d", __func__, channels, MAX_CHN);
        ret = -EINVAL;
        goto out_free_str;
    }

    tmp = (short *)calloc(channels, sizeof(*tmp));
    if (!tmp) {
        ret = -ENOMEM;
        goto out_free_str;
    }

    token = strtok((char *)str, ",");
    i = 0;
    while (token) {
        if (i < channels) {
            tmp[i] = atoi(token);
            i++;
        }

        token = strtok(NULL, ",");
    }

    *array = tmp;

out_free_str:
    free(str);

out:
    return ret;
}

static int prop_get_aes_limit_ratio(xmlNodePtr node, float ratio[2][3])
{
    xmlNodePtr child = node->children;
    int i;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)node->name, "limit_ratio_%d", &i);
    if (ret != 1) {
        ALOGE("%s: invalid name %s", __func__, node->name);
        return -EINVAL;
    }

    if (i < 0 || i > 1) {
        ALOGE("%s: invalid index %d", __func__, i);
        return -EINVAL;
    }

    ret = sscanf((const char *)child->content, "%f,%f,%f", &ratio[i][0], &ratio[i][1],
                 &ratio[i][2]);
    if (ret != 3) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_aes_thd_split_freq(xmlNodePtr node, short freq[4][2])
{
    xmlNodePtr child = node->children;
    int i;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)node->name, "thd_split_freq_%d", &i);
    if (ret != 1) {
        ALOGE("%s: invalid name %s", __func__, node->name);
        return -EINVAL;
    }

    if (i < 0 || i > 3) {
        ALOGE("%s: invalid index %d", __func__, i);
        return -EINVAL;
    }

    ret = sscanf((const char *)child->content, "%hd,%hd", &freq[i][0], &freq[i][1]);
    if (ret != 2) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_aes_thd_sup_degree(xmlNodePtr node, float degree[4][10])
{
    xmlNodePtr child = node->children;
    int i;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)node->name, "thd_sup_degree_%d", &i);
    if (ret != 1) {
        ALOGE("%s: invalid name %s", __func__, node->name);
        return -EINVAL;
    }

    if (i < 0 || i > 3) {
        ALOGE("%s: invalid index %d", __func__, i);
        return -EINVAL;
    }

    ret = sscanf((const char *)child->content, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &degree[i][0],
                 &degree[i][1], &degree[i][2], &degree[i][3], &degree[i][4],
                 &degree[i][5], &degree[i][6], &degree[i][7], &degree[i][8],
                 &degree[i][9]);
    if (ret != 10) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_aes_hard_split_freq(xmlNodePtr node, short freq[5][2])
{
    xmlNodePtr child = node->children;
    int i;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)node->name, "hard_split_freq_%d", &i);
    if (ret != 1) {
        ALOGE("%s: invalid name %s", __func__, node->name);
        return -EINVAL;
    }

    if (i < 0 || i > 4) {
        ALOGE("%s: invalid index %d", __func__, i);
        return -EINVAL;
    }

    ret = sscanf((const char *)child->content, "%hd,%hd", &freq[i][0], &freq[i][1]);
    if (ret != 2) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_aes_hard_threshold(xmlNodePtr node, float threshold[4])
{
    xmlNodePtr child = node->children;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)child->content, "%f,%f,%f,%f", &threshold[0], &threshold[1],
                 &threshold[2], &threshold[3]);
    if (ret != 4) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_nlp_band_pass_thd(xmlNodePtr node, short int para[8][2])
{
    xmlNodePtr child = node->children;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)child->content, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                 &para[0][0], &para[1][0], &para[2][0], &para[3][0], &para[4][0], &para[5][0],
                 &para[6][0], &para[7][0]);
    if (ret != 8) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_nlp_super_est_factor(xmlNodePtr node, short int para[8][2])
{
    xmlNodePtr child = node->children;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)child->content, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                 &para[0][1], &para[1][1], &para[2][1], &para[3][1], &para[4][1], &para[5][1],
                 &para[6][1], &para[7][1]);
    if (ret != 8) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int prop_get_eq_filter_bank(xmlNodePtr node, short coeff[5][13])
{
    xmlNodePtr child = node->children;
    int i;
    int ret;

    if (!child || child->type != XML_TEXT_NODE || !child->content)
        return -EINVAL;

    ret = sscanf((const char *)node->name, "filter_bank_%d", &i);
    if (ret != 1) {
        ALOGE("%s: invalid name %s", __func__, node->name);
        return -EINVAL;
    }

    if (i < 0 || i > 4) {
        ALOGE("%s: invalid index %d", __func__, i);
        return -EINVAL;
    }

    ret = sscanf((const char *)child->content, "%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd",
                 &coeff[i][0], &coeff[i][1], &coeff[i][2], &coeff[i][3], &coeff[i][4],
                 &coeff[i][5], &coeff[i][6], &coeff[i][7], &coeff[i][8], &coeff[i][9],
                 &coeff[i][10], &coeff[i][11], &coeff[i][12]);
    if (ret != 13) {
        ALOGE("%s: invalid content %s", __func__, child->content);
        return -EINVAL;
    }

    return 0;
}

static int load_delay_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKAudioDelayParam *delay;
    const char *name;
    int ret;

    delay = (RKAudioDelayParam *)calloc(1, sizeof(*delay));
    if (!delay)
        return -ENOMEM;

    *param = delay;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "max_frame"))
            ret = prop_get_short(prop, &delay->MaxFrame);
        else if (!strcmp(name, "least_delay"))
            ret = prop_get_short(prop, &delay->LeastDelay);
        else if (!strcmp(name, "jump_frame"))
            ret = prop_get_short(prop, &delay->JumpFrame);
        else if (!strcmp(name, "delay_offset"))
            ret = prop_get_short(prop, &delay->DelayOffset);
        else if (!strcmp(name, "mic_amp_thr"))
            ret = prop_get_short(prop, &delay->MicAmpThr);
        else if (!strcmp(name, "ref_amp_thr"))
            ret = prop_get_short(prop, &delay->RefAmpThr);
        else if (!strcmp(name, "start_freq"))
            ret = prop_get_short(prop, &delay->StartFreq);
        else if (!strcmp(name, "end_freq"))
            ret = prop_get_short(prop, &delay->EndFreq);
        else if (!strcmp(name, "smooth_factor"))
            ret = prop_get_float(prop, &delay->SmoothFactor);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_aes_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKAudioAESParameter *aes;
    const char *name;
    int ret;

    aes = (RKAudioAESParameter *)calloc(1, sizeof(*aes));
    if (!aes)
        return -ENOMEM;

    *param = aes;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "beta_up"))
            ret = prop_get_float(prop, &aes->Beta_Up);
        else if (!strcmp(name, "beta_down"))
            ret = prop_get_float(prop, &aes->Beta_Down);
        else if (!strcmp(name, "beta_up_low"))
            ret = prop_get_float(prop, &aes->Beta_Up_Low);
        else if (!strcmp(name, "beta_down_low"))
            ret = prop_get_float(prop, &aes->Beta_Down_Low);
        else if (!strcmp(name, "low_freq"))
            ret = prop_get_short(prop, &aes->low_freq);
        else if (!strcmp(name, "high_freq"))
            ret = prop_get_short(prop, &aes->high_freq);
        else if (!strcmp(name, "thd_flag"))
            ret = prop_get_short(prop, &aes->THD_Flag);
        else if (!strcmp(name, "hard_flag"))
            ret = prop_get_short(prop, &aes->HARD_Flag);
        else if (!strcmp(name, "limit_ratio_0") || !strcmp(name, "limit_ratio_1"))
            ret = prop_get_aes_limit_ratio(prop, aes->LimitRatio);
        else if (!strcmp(name, "thd_split_freq_0") || !strcmp(name, "thd_split_freq_1") ||
                   !strcmp(name, "thd_split_freq_2") || !strcmp(name, "thd_split_freq_3"))
            ret = prop_get_aes_thd_split_freq(prop, aes->ThdSplitFreq);
        else if (!strcmp(name, "thd_sup_degree_0") || !strcmp(name, "thd_sup_degree_1") ||
                   !strcmp(name, "thd_sup_degree_2") || !strcmp(name, "thd_sup_degree_3"))
            ret = prop_get_aes_thd_sup_degree(prop, aes->ThdSupDegree);
        else if (!strcmp(name, "hard_split_freq_0") || !strcmp(name, "hard_split_freq_1") ||
                   !strcmp(name, "hard_split_freq_2") || !strcmp(name, "hard_split_freq_3") ||
                   !strcmp(name, "hard_split_freq_4"))
            ret = prop_get_aes_hard_split_freq(prop, aes->HardSplitFreq);
        else if (!strcmp(name, "hard_threshold"))
            ret = prop_get_aes_hard_threshold(prop, aes->HardThreshold);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_agc_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKAGCParam *agc;
    const char *name;
    int ret;

    agc = (RKAGCParam *)calloc(1, sizeof(*agc));
    if (!agc)
        return -ENOMEM;

    *param = agc;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "attack_time"))
            ret = prop_get_float(prop, &agc->attack_time);
        else if (!strcmp(name, "release_time"))
            ret = prop_get_float(prop, &agc->release_time);
        else if (!strcmp(name, "attenuate_time"))
            ret = prop_get_float(prop, &agc->attenuate_time);
        else if (!strcmp(name, "max_gain"))
            ret = prop_get_float(prop, &agc->max_gain);
        else if (!strcmp(name, "max_peak"))
            ret = prop_get_float(prop, &agc->max_peak);
        else if (!strcmp(name, "fRth0"))
            ret = prop_get_float(prop, &agc->fRth0);
        else if (!strcmp(name, "fRth1"))
            ret = prop_get_float(prop, &agc->fRth1);
        else if (!strcmp(name, "fRth2"))
            ret = prop_get_float(prop, &agc->fRth2);
        else if (!strcmp(name, "fRk0"))
            ret = prop_get_float(prop, &agc->fRk0);
        else if (!strcmp(name, "fRk1"))
            ret = prop_get_float(prop, &agc->fRk1);
        else if (!strcmp(name, "fRk2"))
            ret = prop_get_float(prop, &agc->fRk2);
        else if (!strcmp(name, "fLineGainDb"))
            ret = prop_get_float(prop, &agc->fLineGainDb);
        else if (!strcmp(name, "swSmL0"))
            ret = prop_get_int(prop, &agc->swSmL0);
        else if (!strcmp(name, "swSmL1"))
            ret = prop_get_int(prop, &agc->swSmL1);
        else if (!strcmp(name, "swSmL2"))
            ret = prop_get_int(prop, &agc->swSmL2);
        else if (!strcmp(name, "fs"))
            ret = prop_get_int(prop, &agc->fs);
        else if (!strcmp(name, "frmlen"))
            ret = prop_get_int(prop, &agc->frmlen);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_anr_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    SKVANRParam *anr;
    const char *name;
    int ret;

    anr = (SKVANRParam *)calloc(1, sizeof(*anr));
    if (!anr)
        return -ENOMEM;

    *param = anr;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "noiseFactor"))
            ret = prop_get_float(prop, &anr->noiseFactor);
        else if (!strcmp(name, "psiMin"))
            ret = prop_get_float(prop, &anr->PsiMin);
        else if (!strcmp(name, "psiMax"))
            ret = prop_get_float(prop, &anr->PsiMax);
        else if (!strcmp(name, "fGmin"))
            ret = prop_get_float(prop, &anr->fGmin);
        else if (!strcmp(name, "swU"))
            ret = prop_get_int(prop, &anr->swU);
        else if (!strcmp(name, "supFreq1"))
            ret = prop_get_short(prop, &anr->Sup_Freq1);
        else if (!strcmp(name, "supFreq2"))
            ret = prop_get_short(prop, &anr->Sup_Freq2);
        else if (!strcmp(name, "supEnergy1"))
            ret = prop_get_float(prop, &anr->Sup_Energy1);
        else if (!strcmp(name, "supEnergy2"))
            ret = prop_get_float(prop, &anr->Sup_Energy2);
        else if (!strcmp(name, "interV"))
            ret = prop_get_short(prop, &anr->InterV);
        else if (!strcmp(name, "biasMin"))
            ret = prop_get_float(prop, &anr->BiasMin);
        else if (!strcmp(name, "updateFrm"))
            ret = prop_get_short(prop, &anr->UpdateFrm);
        else if (!strcmp(name, "splitBand"))
            ret = prop_get_int(prop, &anr->SplitBand);
        else if (!strcmp(name, "tranMode"))
            ret = prop_get_short(prop, &anr->TranMode);
        else if (!strcmp(name, "nPreGammaThr"))
            ret = prop_get_float(prop, &anr->NPreGammaThr);
        else if (!strcmp(name, "nPreZetaThr"))
            ret = prop_get_float(prop, &anr->NPreZetaThr);
        else if (!strcmp(name, "sabsGammaThr0"))
            ret = prop_get_float(prop, &anr->SabsGammaThr0);
        else if (!strcmp(name, "sabsGammaThr1"))
            ret = prop_get_float(prop, &anr->SabsGammaThr1);
        else if (!strcmp(name, "infSmooth"))
            ret = prop_get_float(prop, &anr->InfSmooth);
        else if (!strcmp(name, "probSmooth"))
            ret = prop_get_float(prop, &anr->ProbSmooth);
        else if (!strcmp(name, "compCoeff"))
            ret = prop_get_float(prop, &anr->CompCoeff);
        else if (!strcmp(name, "prioriMin"))
            ret = prop_get_float(prop, &anr->PrioriMin);
        else if (!strcmp(name, "postMax"))
            ret = prop_get_float(prop, &anr->PostMax);
        else if (!strcmp(name, "prioriRatio"))
            ret = prop_get_float(prop, &anr->PrioriRatio);
        else if (!strcmp(name, "prioriRatioLow"))
            ret = prop_get_float(prop, &anr->PrioriRatioLow);
        else if (!strcmp(name, "prioriSmooth"))
            ret = prop_get_float(prop, &anr->PrioriSmooth);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_nlp_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    SKVNLPParameter *nlp;
    const char *name;
    int ret;

    nlp = (SKVNLPParameter *)calloc(1, sizeof(*nlp));
    if (!nlp)
        return -ENOMEM;

    *param = nlp;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "band_pass_thd"))
            ret = prop_get_nlp_band_pass_thd(prop, nlp->g_ashwAecBandNlpPara_16k);
        else if (!strcmp(name, "super_est_factor"))
            ret = prop_get_nlp_super_est_factor(prop, nlp->g_ashwAecBandNlpPara_16k);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_dereverb_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKAudioDereverbParam *dereverb;
    const char *name;
    int ret;

    dereverb = (RKAudioDereverbParam *)calloc(1, sizeof(*dereverb));
    if (!dereverb)
        return -ENOMEM;

    *param = dereverb;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "rlsLg"))
            ret = prop_get_int(prop, &dereverb->rlsLg);
        else if (!strcmp(name, "curveLg"))
            ret = prop_get_int(prop, &dereverb->curveLg);
        else if (!strcmp(name, "delay"))
            ret = prop_get_int(prop, &dereverb->delay);
        else if (!strcmp(name, "forgetting"))
            ret = prop_get_float(prop, &dereverb->forgetting);
        else if (!strcmp(name, "t60"))
            ret = prop_get_float(prop, &dereverb->T60);
        else if (!strcmp(name, "coCoeff"))
            ret = prop_get_float(prop, &dereverb->coCoeff);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_cng_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKCNGParam *cng;
    const char *name;
    int ret;

    cng = (RKCNGParam *)calloc(1, sizeof(*cng));
    if (!cng)
        return -ENOMEM;

    *param = cng;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "fGain"))
            ret = prop_get_float(prop, &cng->fGain);
        else if (!strcmp(name, "fMpy"))
            ret = prop_get_float(prop, &cng->fMpy);
        else if (!strcmp(name, "fSmoothAlpha"))
            ret = prop_get_float(prop, &cng->fSmoothAlpha);
        else if (!strcmp(name, "fSpeechGain"))
            ret = prop_get_float(prop, &cng->fSpeechGain);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_dtd_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKDTDParam *dtd;
    const char *name;
    int ret;

    dtd = (RKDTDParam *)calloc(1, sizeof(*dtd));
    if (!dtd)
        return -ENOMEM;

    *param = dtd;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "ksiThd_high"))
            ret = prop_get_float(prop, &dtd->ksiThd_high);
        else if (!strcmp(name, "ksiThd_low"))
            ret = prop_get_float(prop, &dtd->ksiThd_low);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_eq_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKaudioEqParam *eq;
    const char *name;
    int ret;

    eq = (RKaudioEqParam *)calloc(1, sizeof(*eq));
    if (!eq)
        return -ENOMEM;

    *param = eq;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "shwParaLen"))
            ret = prop_get_int(prop, &eq->shwParaLen);
        else if (!strcmp(name, "filter_bank_0") || !strcmp(name, "filter_bank_1") ||
                   !strcmp(name, "filter_bank_2") || !strcmp(name, "filter_bank_3") ||
                   !strcmp(name, "filter_bank_4"))
            ret = prop_get_eq_filter_bank(prop, eq->pfCoeff);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_howl_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKHOWLParam *howl;
    const char *name;
    int ret;

    howl = (RKHOWLParam *)calloc(1, sizeof(*howl));
    if (!howl)
        return -ENOMEM;

    *param = howl;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "mode"))
            ret = prop_get_short(prop, &howl->howlMode);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_doa_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RKDOAParam *doa;
    const char *name;
    int ret;

    doa = (RKDOAParam *)calloc(1, sizeof(*doa));
    if (!doa)
        return -ENOMEM;

    *param = doa;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "rad"))
            ret = prop_get_float(prop, &doa->rad);
        else if (!strcmp(name, "start_freq"))
            ret = prop_get_short(prop, &doa->start_freq);
        else if (!strcmp(name, "end_freq"))
            ret = prop_get_short(prop, &doa->end_freq);
        else if (!strcmp(name, "lg_num"))
            ret = prop_get_short(prop, &doa->lg_num);
        else if (!strcmp(name, "lg_pitch_num"))
            ret = prop_get_short(prop, &doa->lg_pitch_num);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_aec_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    SKVAECParameter *aec;
    const char *name;
    int ret;

    aec = (SKVAECParameter *)calloc(1, sizeof(*aec));
    if (!aec)
        return -ENOMEM;

    *param = aec;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "pos"))
            ret = prop_get_int(prop, &aec->pos);
        else if (!strcmp(name, "drop_ref_channel"))
            ret = prop_get_int(prop, &aec->drop_ref_channel);
        else if (!strcmp(name, "delay_len"))
            ret = prop_get_int(prop, &aec->delay_len);
        else if (!strcmp(name, "look_ahead"))
            ret = prop_get_int(prop, &aec->look_ahead);
        else if (!strcmp(name, "model_aec_en"))
            ret = prop_get_int(prop, &aec->model_aec_en);
        else if (!strcmp(name, "filter_len"))
            ret = prop_get_short(prop, &aec->filter_len);
        else if (!strcmp(name, "Array_list"))
            ret = prop_get_channel_array(prop, &aec->Array_list);
        else if (!strcmp(name, "delay"))
            ret = load_delay_config(prop, &aec->delay_para);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_bf_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    SKVPreprocessParam *bf;
    const char *name;
    int ret;

    bf = (SKVPreprocessParam *)calloc(1, sizeof(*bf));
    if (!bf)
        return -ENOMEM;

    *param = bf;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "ref_pos"))
            ret = prop_get_int(prop, &bf->ref_pos);
        else if (!strcmp(name, "drop_ref_channel"))
            ret = prop_get_int(prop, &bf->drop_ref_channel);
        else if (!strcmp(name, "targ"))
            ret = prop_get_int(prop, &bf->Targ);
        else if (!strcmp(name, "aes"))
            ret = load_aes_config(prop, &bf->aes_para);
        else if (!strcmp(name, "agc"))
            ret = load_agc_config(prop, &bf->agc_para);
        else if (!strcmp(name, "anr"))
            ret = load_anr_config(prop, &bf->anr_para);
        else if (!strcmp(name, "nlp"))
            ret = load_nlp_config(prop, &bf->nlp_para);
        else if (!strcmp(name, "dereverb"))
            ret = load_dereverb_config(prop, &bf->dereverb_para);
        else if (!strcmp(name, "cng"))
            ret = load_cng_config(prop, &bf->cng_para);
        else if (!strcmp(name, "dtd"))
            ret = load_dtd_config(prop, &bf->dtd_para);
        else if (!strcmp(name, "eq"))
            ret = load_eq_config(prop, &bf->eq_para);
        else if (!strcmp(name, "howl"))
            ret = load_howl_config(prop, &bf->howl_para);
        else if (!strcmp(name, "doa"))
            ret = load_doa_config(prop, &bf->doa_para);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int load_rx_config(xmlNodePtr node, void **param)
{
    xmlNodePtr prop;
    RkaudioRxParam *rx;
    const char *name;
    int ret;

    rx = (RkaudioRxParam *)calloc(1, sizeof(*rx));
    if (!rx)
        return -ENOMEM;

    *param = rx;

    prop = node->xmlChildrenNode;
    while (prop) {
        name = (const char *)prop->name;

        if (!strcmp(name, "anr"))
            ret = load_anr_config(prop, &rx->anr_para);
        else if (!strcmp(name, "howl"))
            ret = load_howl_config(prop, &rx->howl_para);
        else
            ret = 0;

        if (ret)
            return ret;

        prop = prop->next;
    }

    return 0;
}

static int profile_load(struct profile *profile, xmlNodePtr root)
{
    xmlNodePtr node;
    int ret;

    node = root->xmlChildrenNode;
    while (node) {
        if (!xmlStrcmp(node->name, (const xmlChar *)"aec"))
            ret = load_aec_config(node, &profile->param.aec_param);
        else if (!xmlStrcmp(node->name, (const xmlChar *)"bf"))
            ret = load_bf_config(node, &profile->param.bf_param);
        else if (!xmlStrcmp(node->name, (const xmlChar *)"rx"))
            ret = load_rx_config(node, &profile->param.rx_param);
        else
            ret = 0;

        if (ret)
            return ret;

        node = node->next;
    }

    return 0;
}

static void profile_dump_aec(struct profile *profile)
{
    SKVAECParameter *aec;
    RKAudioDelayParam *delay;
    int i;

    aec = profile->param.aec_param;
    if (!aec)
        return;

    ALOGV("AEC:");
    ALOGV("  pos: %d", aec->pos);
    ALOGV("  drop_ref_channel: %d", aec->drop_ref_channel);
    ALOGV("  aec_mode: 0x%x", aec->model_aec_en);
    ALOGV("  delay_len: %d", aec->delay_len);
    ALOGV("  look_ahead: %d", aec->look_ahead);
    ALOGV("  filter_len: %d", aec->filter_len);

    if (aec->Array_list) {
        ALOGV("  Array_list:");
        for (i = 0; i < profile->channels; i++) {
            ALOGV("    channel[%d]: %d", i, aec->Array_list[i]);
        }
    }

    if ((aec->model_aec_en & EN_DELAY) && aec->delay_para) {
        delay = aec->delay_para;
        ALOGV("  DELAY:");
        ALOGV("    maxFrame: 0x%x", delay->MaxFrame);
        ALOGV("    leastDelay: %d", delay->LeastDelay);
        ALOGV("    jumpFrame: %d", delay->JumpFrame);
        ALOGV("    delayOffset: %d", delay->DelayOffset);
        ALOGV("    micAmpThr: 0x%x", delay->MicAmpThr);
        ALOGV("    refAmpThr: %d", delay->RefAmpThr);
        ALOGV("    startFreq: %d", delay->StartFreq);
        ALOGV("    endFreq: %d", delay->EndFreq);
        ALOGV("    smoothFactor: %f", delay->SmoothFactor);
    }
}

static void profile_dump_bf(struct profile *profile)
{
    SKVPreprocessParam *bf;
    RKAudioDereverbParam *dereverb;
    SKVNLPParameter *nlp;
    RKAudioAESParameter *aes;
    SKVANRParam *anr;
    RKAGCParam *agc;
    RKCNGParam *cng;
    RKDTDParam *dtd;
    RKaudioEqParam *eq;
    RKHOWLParam *howl;
    RKDOAParam *doa;
    int i;

    bf = profile->param.bf_param;
    if (!bf)
        return;

    ALOGV("BF:");
    ALOGV("  model_bf_en: 0x%x", bf->model_bf_en);
    ALOGV("  ref_pos: %d", bf->ref_pos);
    ALOGV("  targ: %d", bf->Targ);
    ALOGV("  num_ref_channel: %d", bf->num_ref_channel);
    ALOGV("  drop_ref_channel: %d", bf->drop_ref_channel);

    dereverb = bf->dereverb_para;
    if (dereverb) {
        ALOGV("  DEREVERB:");
        ALOGV("    rlsLg: %d", dereverb->rlsLg);
        ALOGV("    curveLg: %d", dereverb->curveLg);
        ALOGV("    delay: %d", dereverb->delay);
        ALOGV("    forgetting: %f", dereverb->forgetting);
        ALOGV("    t60: %f", dereverb->T60);
        ALOGV("    coCoeff: %f", dereverb->coCoeff);
    }

    nlp = bf->nlp_para;
    if (nlp) {
        ALOGV("  NLP:");
        ALOGV("    BandPassThd   SuperEstFactor");
        for (i = 0; i < 8; i++) {
            ALOGV("         %2d             %2d",
                  nlp->g_ashwAecBandNlpPara_16k[i][0],
                  nlp->g_ashwAecBandNlpPara_16k[i][1]);
        }
    }

    aes = bf->aes_para;
    if (aes) {
        ALOGV("  AES:");
        ALOGV("    beta_up: %f", aes->Beta_Up);
        ALOGV("    beta_down: %f", aes->Beta_Down);
        ALOGV("    beta_up_Low: %f", aes->Beta_Up_Low);
        ALOGV("    beta_down_Low: %f", aes->Beta_Down_Low);
        ALOGV("    low_freq: %d", aes->low_freq);
        ALOGV("    high_freq: %d", aes->high_freq);
        ALOGV("    thd_flag: %d", aes->THD_Flag);
        ALOGV("    hard_flag: %d", aes->HARD_Flag);
        for (i = 0; i < 2; i++) {
            ALOGV("    limit_ratio[%d]: (%f,%f,%f)", i, aes->LimitRatio[i][0],
                  aes->LimitRatio[i][1], aes->LimitRatio[i][2]);
        }
        for (i = 0; i < 4; i++) {
            ALOGV("    thd_split_freq[%d]: (%d,%d)", i, aes->ThdSplitFreq[i][0],
                  aes->ThdSplitFreq[i][1]);
        }
        for (i = 0; i < 4; i++) {
            ALOGV("    thd_sup_degree[%d]: (%f,%f,%f,%f,%f,%f,%f,%f,%f,%f)", i,
                  aes->ThdSupDegree[i][0], aes->ThdSupDegree[i][1], aes->ThdSupDegree[i][2],
                  aes->ThdSupDegree[i][3], aes->ThdSupDegree[i][4], aes->ThdSupDegree[i][5],
                  aes->ThdSupDegree[i][6], aes->ThdSupDegree[i][7], aes->ThdSupDegree[i][8],
                  aes->ThdSupDegree[i][9]);
        }
        for (i = 0; i < 5; i++) {
            ALOGV("    hard_split_freq[%d]: (%d,%d)", i, aes->HardSplitFreq[i][0],
                  aes->HardSplitFreq[i][1]);
        }
        ALOGV("    hard_threshold: %f,%f,%f,%f", aes->HardThreshold[0], aes->HardThreshold[1],
              aes->HardThreshold[2], aes->HardThreshold[3]);
    }

    anr = bf->anr_para;
    if (anr) {
        ALOGV("  ANR:");
        ALOGV("    noiseFactor: %f", anr->noiseFactor);
        ALOGV("    swU: %d", anr->swU);
        ALOGV("    psiMin: %f", anr->PsiMin);
        ALOGV("    psiMax: %f", anr->PsiMax);
        ALOGV("    fGmin: %f", anr->fGmin);
        ALOGV("    supFreq1: %d", anr->Sup_Freq1);
        ALOGV("    supFreq2: %d", anr->Sup_Freq2);
        ALOGV("    supEnergy1: %f", anr->Sup_Energy1);
        ALOGV("    supEnergy2: %f", anr->Sup_Energy2);
        ALOGV("    interV: %d", anr->InterV);
        ALOGV("    biasMin: %f", anr->BiasMin);
        ALOGV("    updateFrm: %d", anr->UpdateFrm);
        ALOGV("    nPreGammaThr: %f", anr->NPreGammaThr);
        ALOGV("    nPreZetaThr: %f", anr->NPreZetaThr);
        ALOGV("    sabsGammaThr0: %f", anr->SabsGammaThr0);
        ALOGV("    sabsGammaThr1: %f", anr->SabsGammaThr1);
        ALOGV("    infSmooth: %f", anr->InfSmooth);
        ALOGV("    probSmooth: %f", anr->ProbSmooth);
        ALOGV("    compCoeff: %f", anr->CompCoeff);
        ALOGV("    prioriMin: %f", anr->PrioriMin);
        ALOGV("    postMax: %f", anr->PostMax);
        ALOGV("    prioriRatio: %f", anr->PrioriRatio);
        ALOGV("    prioriRatioLow: %f", anr->PrioriRatioLow);
        ALOGV("    splitBand: %d", anr->SplitBand);
        ALOGV("    prioriSmooth: %f", anr->PrioriSmooth);
        ALOGV("    tranMode: %d", anr->TranMode);
    }

    agc = bf->agc_para;
    if (agc) {
        ALOGV("  AGC:");
        ALOGV("    attack_time: %f", agc->attack_time);
        ALOGV("    release_time: %f", agc->release_time);
        ALOGV("    attenuate_time: %f", agc->attenuate_time);
        ALOGV("    max_gain: %f", agc->max_gain);
        ALOGV("    max_peak: %f", agc->max_peak);
        ALOGV("    fLineGainDb: %f", agc->fLineGainDb);
        ALOGV("    fRth0: %f", agc->fRth0);
        ALOGV("    fRth1: %f", agc->fRth1);
        ALOGV("    fRth2: %f", agc->fRth2);
        ALOGV("    fRk0: %f", agc->fRk0);
        ALOGV("    fRk1: %f", agc->fRk1);
        ALOGV("    fRk2: %f", agc->fRk2);
        ALOGV("    swSmL0: %d", agc->swSmL0);
        ALOGV("    swSmL1: %d", agc->swSmL1);
        ALOGV("    swSmL2: %d", agc->swSmL2);
    }

    cng = bf->cng_para;
    if (cng) {
        ALOGV("  CNG:");
        ALOGV("    fGain: %f", cng->fGain);
        ALOGV("    fMpy: %f", cng->fMpy);
        ALOGV("    fSmoothAlpha: %f", cng->fSmoothAlpha);
        ALOGV("    fSpeechGain: %f", cng->fSpeechGain);
    }

    dtd = bf->dtd_para;
    if (dtd) {
        ALOGV("  DTD:");
        ALOGV("    ksiThd_high: %f", dtd->ksiThd_high);
        ALOGV("    ksiThd_low: %f", dtd->ksiThd_low);
    }

    eq = bf->eq_para;
    if (eq) {
        ALOGV("  EQ:");
        ALOGV("    shwParaLen: %d", eq->shwParaLen);
        for (i = 0; i < 5; ++i) {
            ALOGV("    pfCoeff[%d]: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", i, eq->pfCoeff[i][0],
                  eq->pfCoeff[i][1], eq->pfCoeff[i][2], eq->pfCoeff[i][3], eq->pfCoeff[i][4],
                  eq->pfCoeff[i][5], eq->pfCoeff[i][6], eq->pfCoeff[i][7], eq->pfCoeff[i][8],
                  eq->pfCoeff[i][9], eq->pfCoeff[i][10], eq->pfCoeff[i][11], eq->pfCoeff[i][12]);
        }
    }

    howl = bf->howl_para;
    if (howl) {
        ALOGV("  HOWL:");
        ALOGV("    mode: %d", howl->howlMode);
    }

    doa = bf->doa_para;
    if (doa) {
        ALOGV("  DOA:");
        ALOGV("    rad: %f", doa->rad);
        ALOGV("    start_freq: %d", doa->start_freq);
        ALOGV("    end_freq: %d", doa->end_freq);
        ALOGV("    lg_num: %d", doa->lg_num);
        ALOGV("    lg_pitch_num: %d", doa->lg_pitch_num);
    }

    ALOGV("  WIND:");
}

static void profile_dump_rx(struct profile *profile)
{
    RkaudioRxParam *rx;
    SKVANRParam *anr;
    RKHOWLParam *howl;

    rx = profile->param.rx_param;
    if (!rx)
        return;

    ALOGV("RX:");
    ALOGV("  model_rx_en: %d", rx->model_rx_en);

    anr = rx->anr_para;
    if (anr) {
        ALOGV("  ANR:");
        ALOGV("    noiseFactor: %f", anr->noiseFactor);
        ALOGV("    swU: %d", anr->swU);
        ALOGV("    psiMin: %f", anr->PsiMin);
        ALOGV("    psiMax: %f", anr->PsiMax);
        ALOGV("    fGmin: %f", anr->fGmin);
        ALOGV("    supFreq1: %d", anr->Sup_Freq1);
        ALOGV("    supFreq2: %d", anr->Sup_Freq2);
        ALOGV("    supEnergy1: %f", anr->Sup_Energy1);
        ALOGV("    supEnergy2: %f", anr->Sup_Energy2);
        ALOGV("    interV: %d", anr->InterV);
        ALOGV("    biasMin: %f", anr->BiasMin);
        ALOGV("    updateFrm: %d", anr->UpdateFrm);
        ALOGV("    nPreGammaThr: %f", anr->NPreGammaThr);
        ALOGV("    nPreZetaThr: %f", anr->NPreZetaThr);
        ALOGV("    sabsGammaThr0: %f", anr->SabsGammaThr0);
        ALOGV("    sabsGammaThr1: %f", anr->SabsGammaThr1);
        ALOGV("    infSmooth: %f", anr->InfSmooth);
        ALOGV("    probSmooth: %f", anr->ProbSmooth);
        ALOGV("    compCoeff: %f", anr->CompCoeff);
        ALOGV("    prioriMin: %f", anr->PrioriMin);
        ALOGV("    postMax: %f", anr->PostMax);
        ALOGV("    prioriRatio: %f", anr->PrioriRatio);
        ALOGV("    prioriRatioLow: %f", anr->PrioriRatioLow);
        ALOGV("    splitBand: %d", anr->SplitBand);
        ALOGV("    prioriSmooth: %f", anr->PrioriSmooth);
        ALOGV("    tranMode: %d", anr->TranMode);
    }

    howl = rx->howl_para;
    if (howl) {
        ALOGV("  HOWL:");
        ALOGV("    mode: %d", howl->howlMode);
    }
}

static void profile_set_model(struct profile *profile, uint32_t enabled_mask)
{
    SKVAECParameter *aec = profile->param.aec_param;
    SKVPreprocessParam *bf = profile->param.bf_param;
    RkaudioRxParam *rx = profile->param.rx_param;

    if (enabled_mask & BIT(PREPROC_AEC_NORMAL)) {
        profile->param.model_en |= RKAUDIO_EN_AEC;
    }

    if (enabled_mask & BIT(PREPROC_AEC_DELAY)) {
        profile->param.model_en |= RKAUDIO_EN_AEC;
        aec->model_aec_en |= EN_DELAY;
    }

    if (enabled_mask & BIT(PREPROC_AEC_ARRAY_RESET)) {
        profile->param.model_en |= RKAUDIO_EN_AEC;
        aec->model_aec_en |= EN_ARRAY_RESET;
    }

    if (enabled_mask & BIT(PREPROC_BF_FAST_AEC)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Fastaec;
    }

    if (enabled_mask & BIT(PREPROC_BF_WAKEUP)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Wakeup;
    }

    if (enabled_mask & BIT(PREPROC_BF_DEREVERB)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Dereverberation;
    }

    if (enabled_mask & BIT(PREPROC_BF_NLP)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Nlp;
    }

    if (enabled_mask & BIT(PREPROC_BF_AES)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_AES;
    }

    if (enabled_mask & BIT(PREPROC_BF_AGC)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Agc;
    }

    if (enabled_mask & BIT(PREPROC_BF_ANR)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Anr;
    }

    if (enabled_mask & BIT(PREPROC_BF_GSC)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_GSC;
    }

    if (enabled_mask & BIT(PREPROC_BF_GSC_METHOD)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= GSC_Method;
    }

    if (enabled_mask & BIT(PREPROC_BF_FIX)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_Fix;
    }

    if (enabled_mask & BIT(PREPROC_BF_DTD)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_STDT;
    }

    if (enabled_mask & BIT(PREPROC_BF_CNG)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_CNG;
    }

    if (enabled_mask & BIT(PREPROC_BF_EQ)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_EQ;
    }

    if (enabled_mask & BIT(PREPROC_BF_CHN_SELECT)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_CHN_SELECT;
    }

    if (enabled_mask & BIT(PREPROC_BF_HOWLING)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_HOWLING;
    }

    if (enabled_mask & BIT(PREPROC_BF_DOA)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_DOA;
    }

    if (enabled_mask & BIT(PREPROC_BF_WIND)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_WIND;
    }

    if (enabled_mask & BIT(PREPROC_BF_AINR)) {
        profile->param.model_en |= RKAUDIO_EN_BF;
        bf->model_bf_en |= EN_AINR;
    }

    if (enabled_mask & BIT(PREPROC_RX_ANR)) {
        profile->param.model_en |= RKAUDIO_EN_RX;
        rx->model_rx_en |= EN_RX_Anr;
    }

    if (enabled_mask & BIT(PREPROC_RX_HOWLING)) {
        profile->param.model_en |= RKAUDIO_EN_RX;
        rx->model_rx_en |= EN_RX_HOWLING;
    }
}

static void profile_set_bf_ref_channel(struct profile *profile)
{
    SKVPreprocessParam *bf = profile->param.bf_param;

    bf->num_ref_channel = profile->channels_ref;
}

int profile_init(struct profile *profile, int rate, int frames, int channels_src, int channels_ref,
                 uint32_t enabled_mask)
{
    xmlDocPtr config;
    xmlNodePtr root;
    int ret;

    config = xmlParseFile(PROFILE_PATH);
    if (!config) {
        ALOGE("No profile");
        ret = -EINVAL;
        goto err;
    }

    root = xmlDocGetRootElement(config);
    if (!root) {
        ALOGE("Invalid profile");
        ret = -EINVAL;
        goto cleanup_config;
    }

    ret = profile_load(profile, root);
    if (ret)
        goto cleanup_profile;

    profile->rate = rate;
    profile->frames = frames;
    profile->channels_src = channels_src;
    profile->channels_ref = channels_ref;
    profile->channels = channels_ref + channels_src;
    profile_set_model(profile, enabled_mask);
    profile_set_bf_ref_channel(profile);
    profile_dump(profile);

    xmlFreeDoc(config);

    return 0;

cleanup_profile:
    profile_release(profile);

cleanup_config:
    xmlFreeDoc(config);

err:
    return ret;
}

void profile_release(struct profile *profile)
{
    SKVAECParameter *aec;
    SKVPreprocessParam *bf;
    RkaudioRxParam *rx;

    if (!profile)
        return;

    aec = profile->param.aec_param;
    if (aec) {
        SAFE_FREE(aec->Array_list);
        SAFE_FREE(aec->delay_para)
        free(aec);
    }
    profile->param.aec_param = NULL;

    bf = profile->param.bf_param;
    if (bf) {
        SAFE_FREE(bf->dereverb_para)
        SAFE_FREE(bf->aes_para)
        SAFE_FREE(bf->nlp_para)
        SAFE_FREE(bf->anr_para)
        SAFE_FREE(bf->agc_para)
        SAFE_FREE(bf->cng_para)
        SAFE_FREE(bf->dtd_para)
        SAFE_FREE(bf->eq_para)
        SAFE_FREE(bf->howl_para)
        SAFE_FREE(bf->doa_para)
        free(bf);
    }
    profile->param.bf_param = NULL;

    rx = profile->param.rx_param;
    if (rx) {
        SAFE_FREE(rx->anr_para);
        SAFE_FREE(rx->howl_para);
        free(rx);
    }
    profile->param.rx_param = NULL;

    profile->rate = 0;
    profile->frames = 0;
    profile->channels_src = 0;
    profile->channels_ref = 0;
    profile->channels = 0;
}

void profile_dump(struct profile *profile)
{
    if (!profile)
        return;

    ALOGV("rate: %d", profile->rate);
    ALOGV("frames: %d", profile->frames);
    ALOGV("chanels: %d", profile->channels);
    ALOGV("channels_src: %d", profile->channels_src);
    ALOGV("channels_ref: %d", profile->channels_ref);
    ALOGV("model_en: 0x%x", profile->param.model_en);

    profile_dump_aec(profile);
    profile_dump_bf(profile);
    profile_dump_rx(profile);
}
