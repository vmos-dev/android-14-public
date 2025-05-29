/*
 * Copyright (C) 2020 Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
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
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "drm-vop-3399"

#include "rockchip/platform/drmvop3399.h"
#include "drmdevice.h"

#include "im2d.hpp"

#include <log/log.h>

namespace android {

#define ALIGN_DOWN( value, base)	(value & (~(base-1)) )
#ifndef ALIGN
#define ALIGN( value, base ) (((value) + ((base) - 1)) & ~((base) - 1))
#endif

void Vop3399::Init(){

  ctx.state.bMultiAreaEnable = hwc_get_bool_property("vendor.hwc.multi_area_enable","true");

  ctx.state.bRgaPolicyEnable = hwc_get_int_property("vendor.hwc.enable_rga_policy","1") > 0;

}

bool Vop3399::SupportPlatform(uint32_t soc_id){
  switch(soc_id){
    case 0x3399:
      return true;
    default:
      break;
  }
  return false;
}

int Vop3399::TryHwcPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc,
    bool gles_policy) {

  int ret;
  // Get PlaneGroup
  if(plane_groups.size() == 0){
    ALOGE("%s,line=%d can't get plane_groups size=%zu",__FUNCTION__,__LINE__,plane_groups.size());
    return -1;
  }

  // Init context
  InitContext(layers,plane_groups,crtc,gles_policy);

  // Try to match overlay policy
  if(ctx.state.setHwcPolicy.count(HWC_OVERLAY_POLICY)){
    ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match overlay policy fail, try to match other policy.");
      TryMix();
    }
  }

  // Try to match mix policy
  if(ctx.state.setHwcPolicy.count(HWC_MIX_POLICY)){
    ret = TryMixPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else{
      ALOGD_IF(LogLevel(DBG_DEBUG),"Match mix policy fail, try to match other policy.");
      ctx.state.setHwcPolicy.insert(HWC_GLES_POLICY);
    }
  }

  // Try to match GLES policy
  if(ctx.state.setHwcPolicy.count(HWC_GLES_POLICY)){
    ret = TryGLESPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }

  ALOGE("%s,%d Can't match HWC policy",__FUNCTION__,__LINE__);
  return -1;
}

bool Vop3399::HasLayer(std::vector<DrmHwcLayer*>& layer_vector,DrmHwcLayer *layer){
        for (std::vector<DrmHwcLayer*>::const_iterator iter = layer_vector.begin();
               iter != layer_vector.end(); ++iter) {
            if((*iter)->uId_==layer->uId_)
                return true;
          }

          return false;
}



bool Vop3399::IsRec1IntersectRec2(hwc_rect_t* rec1, hwc_rect_t* rec2){
    int iMaxLeft,iMaxTop,iMinRight,iMinBottom;
    ALOGD_IF(LogLevel(DBG_DEBUG),"IsRec1IntersectRec2: rec1[%d,%d,%d,%d],rec2[%d,%d,%d,%d]",rec1->left,rec1->top,
        rec1->right,rec1->bottom,rec2->left,rec2->top,rec2->right,rec2->bottom);

    iMaxLeft = rec1->left > rec2->left ? rec1->left: rec2->left;
    iMaxTop = rec1->top > rec2->top ? rec1->top: rec2->top;
    iMinRight = rec1->right <= rec2->right ? rec1->right: rec2->right;
    iMinBottom = rec1->bottom <= rec2->bottom ? rec1->bottom: rec2->bottom;

    if(iMaxLeft > iMinRight || iMaxTop > iMinBottom)
        return false;
    else
        return true;

    return false;
}

bool Vop3399::IsLayerCombine(DrmHwcLayer * layer_one,DrmHwcLayer * layer_two){
    if(!ctx.state.bMultiAreaEnable)
      return false;

    //multi region only support RGBA888 RGB888 565
    if(layer_one->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12
        || layer_two->iFormat_ >= HAL_PIXEL_FORMAT_YCrCb_NV12
        || (layer_one->iFormat_ != layer_two->iFormat_)
        || (layer_one->bAfbcd_ != layer_two->bAfbcd_)
        || layer_one->alpha!= layer_two->alpha
        || (layer_one->bScale_ || layer_two->bScale_)
        || IsRec1IntersectRec2(&layer_one->display_frame,&layer_two->display_frame)
        )
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"is_layer_combine layer one alpha=%d,is_scale=%d",layer_one->alpha,layer_one->bScale_);
        ALOGD_IF(LogLevel(DBG_DEBUG),"is_layer_combine layer two alpha=%d,is_scale=%d",layer_two->alpha,layer_two->bScale_);
        return false;
    }

    return true;
}

int Vop3399::CombineLayer(LayerMap& layer_map,std::vector<DrmHwcLayer*> &layers,uint32_t iPlaneSize){

    /*Group layer*/
    int zpos = 0;
    size_t i,j;
    uint32_t sort_cnt=0;
    bool is_combine = false;

    layer_map.clear();

    for (i = 0; i < layers.size(); ) {
        if(!layers[i]->bUse_)
            continue;

        sort_cnt=0;
        if(i == 0)
        {
            layer_map[zpos].push_back(layers[0]);
        }

        for(j = i+1; j < layers.size(); j++) {
            DrmHwcLayer *layer_one = layers[j];
            //layer_one.index = j;
            is_combine = false;

            for(size_t k = 0; k <= sort_cnt; k++ ) {
                DrmHwcLayer *layer_two = layers[j-1-k];
                //layer_two.index = j-1-k;
                //juage the layer is contained in layer_vector
                bool bHasLayerOne = HasLayer(layer_map[zpos],layer_one);
                bool bHasLayerTwo = HasLayer(layer_map[zpos],layer_two);

                //If it contain both of layers,then don't need to go down.
                if(bHasLayerOne && bHasLayerTwo)
                    continue;

                if(IsLayerCombine(layer_one,layer_two)) {
                    //append layer into layer_vector of layer_map_.
                    if(!bHasLayerOne && !bHasLayerTwo)
                    {
                        layer_map[zpos].emplace_back(layer_one);
                        layer_map[zpos].emplace_back(layer_two);
                        is_combine = true;
                    }
                    else if(!bHasLayerTwo)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_one->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_two))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_two);
                    }
                    else if(!bHasLayerOne)
                    {
                        is_combine = true;
                        for(std::vector<DrmHwcLayer*>::const_iterator iter= layer_map[zpos].begin();
                            iter != layer_map[zpos].end();++iter)
                        {
                            if((*iter)->uId_==layer_two->uId_)
                                    continue;

                            if(!IsLayerCombine(*iter,layer_one))
                            {
                                is_combine = false;
                                break;
                            }
                        }

                        if(is_combine)
                            layer_map[zpos].emplace_back(layer_one);
                    }
                }

                if(!is_combine)
                {
                    //if it cann't combine two layer,it need start a new group.
                    if(!bHasLayerOne)
                    {
                        zpos++;
                        layer_map[zpos].emplace_back(layer_one);
                    }
                    is_combine = false;
                    break;
                }
             }
             sort_cnt++; //update sort layer count
             if(!is_combine)
             {
                break;
             }
        }

        if(is_combine)  //all remain layer or limit MOST_WIN_ZONES layer is combine well,it need start a new group.
            zpos++;
        if(sort_cnt)
            i+=sort_cnt;    //jump the sort compare layers.
        else
            i++;
    }

  // RK3399 sort layer by xpos
  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        if(iter->second.size() > 1) {
            for(i = 0; i < iter->second.size()-1; i++) {
                for(j = i + 1; j < iter->second.size(); j++) {
                     if(iter->second[i]->display_frame.left > iter->second[j]->display_frame.left) {
                        ALOGD_IF(LogLevel(DBG_DEBUG),"swap %d and %d",iter->second[i]->uId_,iter->second[j]->uId_);
                        std::swap(iter->second[i],iter->second[j]);
                     }
                 }
            }
        }
  }


  for (LayerMap::iterator iter = layer_map.begin();
       iter != layer_map.end(); ++iter) {
        ALOGD_IF(LogLevel(DBG_DEBUG),"layer map id=%d,size=%zu",iter->first,iter->second.size());
        for(std::vector<DrmHwcLayer*>::const_iterator iter_layer = iter->second.begin();
            iter_layer != iter->second.end();++iter_layer)
        {
             ALOGD_IF(LogLevel(DBG_DEBUG),"\tlayer id=%u , name=%s",(*iter_layer)->uId_,(*iter_layer)->sLayerName_.c_str());
        }
  }

    if((int)layer_map.size() > iPlaneSize)
    {
        ALOGD_IF(LogLevel(DBG_DEBUG),"map size=%zu should not bigger than plane size=%d", layer_map.size(), iPlaneSize);
        return -1;
    }

    return 0;

}

int Vop3399::MatchPlane(std::vector<DrmCompositionPlane> *composition_planes,
                   std::vector<PlaneGroup *> &plane_groups,
                   DrmCompositionPlane::Type type, DrmCrtc *crtc,
                   std::pair<int, std::vector<DrmHwcLayer*>> layers, int zpos, bool match_best=false) {

  uint32_t layer_size = layers.second.size();
  bool b_yuv=false,b_scale=false,b_alpha=false,b_hdr2sdr=false,b_afbc=false;
  std::vector<PlaneGroup *> ::const_iterator iter;
  uint64_t rotation = 0;
  uint64_t alpha = 0xFF;
  uint16_t eotf = TRADITIONAL_GAMMA_SDR;
  bool bMulArea = layer_size > 0 ? true : false;
  DrmDevice *drm = crtc->getDrmDevice();
  bool bHdrSupport = false;
  DrmConnector *connector = drm->GetConnectorForDisplay(crtc->display());
  if(connector){
    bHdrSupport = connector->is_hdmi_support_hdr() && ctx.support.iHdrCnt > 0;
  }

  bool afbc_used=false;
  
  //loop plane groups.
  for (iter = plane_groups.begin();
    iter != plane_groups.end(); ++iter) {
      ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,last zpos=%d,group(%" PRIu64 ") zpos=%d,group bUse=%d,crtc=0x%x,"
                                   "current_crtc_=0x%x,possible_crtcs=0x%x",
                                   __LINE__, zpos, (*iter)->share_id, (*iter)->zpos, (*iter)->bUse,
                                   (1<<crtc->pipe()), (*iter)->current_crtc_,(*iter)->possible_crtcs);
      //find the match zpos plane group
      if(!(*iter)->bUse && !(*iter)->bReserved && (((1<<crtc->pipe()) & (*iter)->current_crtc_) > 0))
      {
          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,layer_size=%d,planes size=%zu",__LINE__,layer_size,(*iter)->planes.size());
          bool afbc_skip=afbc_used;

          //find the match combine layer count with plane size.
          if(layer_size <= (*iter)->planes.size())
          {
              uint32_t combine_layer_count = 0;

              //loop layer
              for(std::vector<DrmHwcLayer*>::const_iterator iter_layer= layers.second.begin();
                  iter_layer != layers.second.end();++iter_layer)
              {
                  //reset is_match to false
                  (*iter_layer)->bMatch_ = false;

                  if(match_best){
                      if(!((*iter)->win_type & (*iter_layer)->iBestPlaneType)){
                          ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d, plane_group win-type = 0x%" PRIx64 " , layer best-type = %x, not match ",
                          __LINE__,(*iter)->win_type, (*iter_layer)->iBestPlaneType);
                          continue;
                      }
                  }

                  //loop plane
                  for(std::vector<DrmPlane*> ::const_iterator iter_plane=(*iter)->planes.begin();
                      !(*iter)->planes.empty() && iter_plane != (*iter)->planes.end(); ++iter_plane)
                  {
                      ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d,crtc=0x%x,%s is_use=%d,possible_crtc_mask=0x%x",__LINE__,(1<<crtc->pipe()),
                              (*iter_plane)->name(),(*iter_plane)->is_use(),(*iter_plane)->get_possible_crtc_mask());


                      if(!(*iter_plane)->is_use() && (*iter_plane)->GetCrtcSupported(*crtc))
                      {
                          bool bNeed = false;

                          // Format
                          if((*iter_plane)->is_support_format((*iter_layer)->uFourccFormat_,(*iter_layer)->bAfbcd_)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support fourcc=0x%x afbcd = %d",(*iter_plane)->name(),(*iter_layer)->uFourccFormat_,(*iter_layer)->bAfbcd_);
                            continue;
                          }

                          // Input info
                          int input_w = (int)((*iter_layer)->source_crop.right - (*iter_layer)->source_crop.left);
                          int input_h = (int)((*iter_layer)->source_crop.bottom - (*iter_layer)->source_crop.top);
                          if((*iter_plane)->is_support_input(input_w,input_h)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support intput (%d,%d), max_input_range is (%d,%d)",
                                    (*iter_plane)->name(),input_w,input_h,(*iter_plane)->get_input_w_max(),(*iter_plane)->get_input_h_max());
                            continue;

                          }

                          // Output info
                          int output_w = (*iter_layer)->display_frame.right - (*iter_layer)->display_frame.left;
                          int output_h = (*iter_layer)->display_frame.bottom - (*iter_layer)->display_frame.top;

                          if((*iter_plane)->is_support_output(output_w,output_h)){
                            bNeed = true;
                          }else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support output (%d,%d), max_input_range is (%d,%d)",
                                    (*iter_plane)->name(),output_w,output_h,(*iter_plane)->get_output_w_max(),(*iter_plane)->get_output_h_max());
                            continue;

                          }

                          // Scale
                          if((*iter_plane)->is_support_scale((*iter_layer)->fHScaleMul_) &&
                              (*iter_plane)->is_support_scale((*iter_layer)->fVScaleMul_))
                            bNeed = true;
                          else{
                            ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support scale factor(%f,%f)",
                                    (*iter_plane)->name(), (*iter_layer)->fHScaleMul_, (*iter_layer)->fVScaleMul_);
                            continue;
                          }

                          // Alpha
                          if ((*iter_layer)->blending == DrmHwcBlending::kPreMult)
                              alpha = (*iter_layer)->alpha;
                          b_alpha = ((*iter_plane)->alpha_property().id()?true:false)||
                                    ((*iter_plane)->alpha_property_vop1_kernel4_19().id()?true:false);
                          if(alpha != 0xFF)
                          {
                              if(!b_alpha)
                              {
                                  ALOGV("layer id=%d, %s",(*iter_layer)->uId_,(*iter_plane)->name());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"%s can't support alpha,layer alpha=0x%x,alpha id=%d,,alpha_vop1_kernel4.19 id=%d",
                                          (*iter_plane)->name(),(*iter_layer)->alpha,(*iter_plane)->alpha_property().id(),(*iter_plane)->alpha_property_vop1_kernel4_19().id());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          // HDR
                          eotf = (*iter_layer)->uEOTF;
                          b_hdr2sdr = crtc->get_hdr();
                          if(bHdrSupport && eotf != TRADITIONAL_GAMMA_SDR)
                          {
                              if(!b_hdr2sdr)
                              {
                                  ALOGV("layer id=%d, %s",(*iter_layer)->uId_,(*iter_plane)->name());
                                  ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support etof,layer eotf=%d,hdr2sdr=%d",
                                          (*iter_plane)->name(),(*iter_layer)->uEOTF,(*iter_plane)->get_hdr2sdr());
                                  continue;
                              }
                              else
                                  bNeed = true;
                          }

                          // Only YUV use Cluster rotate
                          if((*iter_plane)->is_support_transform((*iter_layer)->transform)){
                            if((*iter_layer)->bYuv_ && (DRM_MODE_REFLECT_Y & (*iter_layer)->transform)){
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support yuv DRM_MODE_REFLECT_Y",
                                        (*iter_plane)->name());
                              continue;
                            }
                          }else{
                              ALOGD_IF(LogLevel(DBG_DEBUG),"%s cann't support layer transform 0x%x, support 0x%x",
                                      (*iter_plane)->name(), (*iter_layer)->transform,(*iter_plane)->get_transform());
                              continue;
                          }
                          if((*iter_layer)->bAfbcd_ && afbc_skip)
                            continue;
                          if(!(*iter_layer)->bMatch_ || (*iter)->afbc_layer_used==-1){
                              (*iter)->afbc_layer_used=(*iter_layer)->bAfbcd_?1:0;
                              if((*iter)->afbc_layer_used==1)
                                afbc_used=true;
                          }else if((*iter)->afbc_layer_used ==0 && (*iter_layer)->bAfbcd_){
                              continue;
                          }else if((*iter)->afbc_layer_used ==1 && !(*iter_layer)->bAfbcd_){
                              continue;
                          }

                          ALOGD_IF(LogLevel(DBG_DEBUG),"MatchPlane: match layer id=%d, %s, zops = %d",(*iter_layer)->uId_,
                              (*iter_plane)->name(),zpos);
                          //Find the match plane for layer,it will be commit.
                          composition_planes->emplace_back(type, (*iter_plane), crtc, (*iter_layer)->iDrmZpos_);
                          (*iter_layer)->bMatch_ = true;
                          (*iter_plane)->set_use(true);
                          composition_planes->back().set_zpos(zpos);
                          combine_layer_count++;
                          break;
                      }
                  }
              }
              if(combine_layer_count == layer_size)
              {
                  ALOGD_IF(LogLevel(DBG_DEBUG),"line=%d all match",__LINE__);
                  (*iter)->bUse = true;
                  return 0;
              }
          }
      }

  }


  return -1;
}

void Vop3399::ResetPlaneGroups(std::vector<PlaneGroup *> &plane_groups){
  for (auto &plane_group : plane_groups){
    for(auto &p : plane_group->planes)
      p->set_use(false);
      plane_group->bUse = false;
      plane_group->afbc_layer_used = -1;
  }
  return;
}

void Vop3399::ResetLayer(std::vector<DrmHwcLayer*>& layers){
    for (auto &drmHwcLayer : layers){
      drmHwcLayer->bMatch_ = false;
    }
    return;
}

int Vop3399::MatchPlanes(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  composition->clear();
  LayerMap layer_map;
  if(CombineLayer(layer_map, layers, plane_groups.size())){
    HWC2_ALOGD_IF_DEBUG("Combine layer Failed!");
    return -1;
  }

  // Fill up the remaining planes
  int zpos = 0;
  for (auto i = layer_map.begin(); i != layer_map.end(); i = layer_map.erase(i)) {
    int ret = MatchPlane(composition, plane_groups, DrmCompositionPlane::Type::kLayer,
                      crtc, std::make_pair(i->first, i->second),zpos);
    if (ret) {
      ALOGD_IF(LogLevel(DBG_DEBUG),"Failed to match all layer, try other HWC policy ret = %d, line = %d",ret,__LINE__);
      ResetLayer(layers);
      ResetPlaneGroups(plane_groups);
      composition->clear();
      return ret;
    }

    zpos++;
  }
  return 0;
}
int  Vop3399::GetPlaneGroups(DrmCrtc *crtc, std::vector<PlaneGroup *>&out_plane_groups){
  DrmDevice *drm = crtc->getDrmDevice();
  out_plane_groups.clear();
  std::vector<PlaneGroup *> all_plane_groups = drm->GetPlaneGroups();
  for(auto &plane_group : all_plane_groups){
    if(plane_group->acquire(1 << crtc->pipe()))
      out_plane_groups.push_back(plane_group);
  }

  return out_plane_groups.size() > 0 ? 0 : -1;
}

void Vop3399::ResetLayerFromTmpExceptFB(std::vector<DrmHwcLayer*>& layers,
                                              std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = layers.begin(); i != layers.end();){
      if((*i)->bFbTarget_){
          tmp_layers.emplace_back(std::move(*i));
          i = layers.erase(i);
          continue;
      }
      i++;
  }
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();){
    if((*i)->bFbTarget_){
      i++;
      continue;
    }
    layers.emplace_back(std::move(*i));
    i = tmp_layers.erase(i);
  }
  //sort
  for (auto i = layers.begin(); i != layers.end()-1; i++){
     for (auto j = i+1; j != layers.end(); j++){
        if((*i)->iZpos_ > (*j)->iZpos_){
           std::swap(*i, *j);
        }
     }
  }

  return;
}


void Vop3399::ResetLayerFromTmp(std::vector<DrmHwcLayer*>& layers,
                                              std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();){
         layers.emplace_back(std::move(*i));
         i = tmp_layers.erase(i);
     }
     //sort
     for (auto i = layers.begin(); i != layers.end()-1; i++){
         for (auto j = i+1; j != layers.end(); j++){
             if((*i)->iZpos_ > (*j)->iZpos_){
                 std::swap(*i, *j);
             }
         }
     }

    return;
}

void Vop3399::MoveFbToTmp(std::vector<DrmHwcLayer*>& layers,
                                       std::vector<DrmHwcLayer*>& tmp_layers){
  for (auto i = layers.begin(); i != layers.end();){
      if((*i)->bFbTarget_){
          tmp_layers.emplace_back(std::move(*i));
          i = layers.erase(i);
          continue;
      }
      i++;
  }
  int zpos = 0;
  for(auto &layer : layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }

  zpos = 0;
  for(auto &layer : tmp_layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }
  return;
}

void Vop3399::OutputMatchLayer(int iFirst, int iLast,
                                          std::vector<DrmHwcLayer *>& layers,
                                          std::vector<DrmHwcLayer *>& tmp_layers){

  if(iFirst < 0 || iLast < 0 || iFirst > iLast)
  {
      ALOGE("invalid value iFirst=%d, iLast=%d", iFirst, iLast);
      return;
  }

  int interval = layers.size()-1-iLast;
  ALOGD_IF(LogLevel(DBG_DEBUG), "OutputMatchLayer iFirst=%d,iLast,=%d,interval=%d",iFirst,iLast,interval);
  for (auto i = layers.begin() + iFirst; i != layers.end() - interval;)
  {
      //move gles layers
      tmp_layers.emplace_back(std::move(*i));
      i = layers.erase(i);
  }

  //add fb layer.
  int pos = iFirst;
  for (auto i = tmp_layers.begin(); i != tmp_layers.end();)
  {
      if((*i)->bFbTarget_){
          layers.insert(layers.begin() + pos, std::move(*i));
          pos++;
          i = tmp_layers.erase(i);
          continue;
      }
      i++;
  }
  int zpos = 0;
  for(auto &layer : layers){
    layer->iDrmZpos_ = zpos;
    zpos++;
  }
  return;
}
int Vop3399::TryOverlayPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> tmp_layers;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }
  return 0;
}

int Vop3399::TryRgaOverlayPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
//Android 11 disable RGA Policy
//Android 11 当前 RGA 策略编译无法通过，暂时跳过
#if PLATFORM_SDK_VERSION > 30
  if(!ctx.state.bRgaPolicyEnable){
    HWC2_ALOGD_IF_DEBUG("bRgaPolicyEnable=%d skip TryRgaOverlayPolicy", ctx.state.bRgaPolicyEnable);
    return -1;
  }
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> tmp_layers;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);

  bool rga_layer_ready = false;
  bool use_laster_rga_layer = false;
  std::shared_ptr<DrmBuffer> dst_buffer;
  static uint64_t last_buffer_id = 0;
  int releaseFence = -1;
  rga_buffer_t src;
  rga_buffer_t dst;
  rga_buffer_t pat;
  im_rect src_rect;
  im_rect dst_rect;
  im_rect pat_rect;
  memset(&src, 0, sizeof(rga_buffer_t));
  memset(&dst, 0, sizeof(rga_buffer_t));
  memset(&pat, 0, sizeof(rga_buffer_t));
  memset(&src_rect, 0, sizeof(im_rect));
  memset(&dst_rect, 0, sizeof(im_rect));
  memset(&pat_rect, 0, sizeof(im_rect));
  int usage = 0;

  for(auto &drmLayer : layers){
    if(drmLayer->bYuv_){
        if(drmLayer->bAfbcd_)
          continue;
        if(last_buffer_id != drmLayer->uBufferId_){

          // TODO: RGA too slow for high resolution 60 fps()
          if((ctx.state.iDisplayWidth_*ctx.state.iDisplayHeight_)>(1536*2048)){
            HWC2_ALOGD_IF_DEBUG("RGA too slow for iWidth_=%d iHeight_=%d yuv layer",
                        drmLayer->iWidth_,drmLayer->iHeight_);
            continue;
          }

          bool rga_scale_max = false;
          // RGA 有缩放倍数限制
          if((drmLayer->fHScaleMul_ < (1.0/16.0) ||
              drmLayer->fHScaleMul_ > 16.0   ||
              drmLayer->fVScaleMul_ < (1.0/16.0) ||
              drmLayer->fVScaleMul_ > 16.0)){
              rga_scale_max = true;
          }

          bool yuv_10bit = false;
          switch(drmLayer->iFormat_){
          case HAL_PIXEL_FORMAT_YUV420_10BIT_I:
          case HAL_PIXEL_FORMAT_YCrCb_NV12_10:
            yuv_10bit = true;
            break;
          default:
            break;
          }

          if(yuv_10bit){
            // RGA 内部特殊修改，需要满足byte_stride 64对齐，width 2对齐
            dst_buffer = rgaBufferQueue_->DequeueDrmBuffer(ALIGN(ctx.state.iDisplayWidth_, 2),
                                                           ctx.state.iDisplayHeight_,
                                                           HAL_PIXEL_FORMAT_YCrCb_NV12_10,
                                                           RK_GRALLOC_USAGE_STRIDE_ALIGN_64 |
                                                           MALI_GRALLOC_USAGE_NO_AFBC |
                                                           RK_GRALLOC_USAGE_WITHIN_4G,
                                                           "RGA-SurfaceView");
          }else{
            dst_buffer = rgaBufferQueue_->DequeueDrmBuffer(ctx.state.iDisplayWidth_,
                                                           ctx.state.iDisplayHeight_,
                                                           HAL_PIXEL_FORMAT_YCrCb_NV12,
                                                           RK_GRALLOC_USAGE_STRIDE_ALIGN_16 |
                                                           MALI_GRALLOC_USAGE_NO_AFBC |
                                                           RK_GRALLOC_USAGE_WITHIN_4G,
                                                           "RGA-SurfaceView");

          }

          if(dst_buffer == NULL){
            HWC2_ALOGD_IF_DEBUG("DequeueDrmBuffer fail!, skip this policy.");
            continue;
          }

          // Set src buffer info
          src.fd      = drmLayer->iFd_;
          src.width   = drmLayer->iWidth_;
          src.height  = drmLayer->iHeight_;
          src.hstride = drmLayer->iHeightStride_;
          src.format  = drmLayer->iFormat_;

          // RGA 的特殊修改，需要通过 wstride
          if(drmLayer->uFourccFormat_ == DRM_FORMAT_NV15)
            src.wstride = drmLayer->iByteStride_;
          else
            src.wstride = drmLayer->iStride_;

          if(drmLayer->iFormat_ == HAL_PIXEL_FORMAT_YUV420_8BIT_I){
            src.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
          }else if(drmLayer->iFormat_ == HAL_PIXEL_FORMAT_YUV420_10BIT_I){
            src.format = HAL_PIXEL_FORMAT_YCrCb_NV12_10;
          }

          // Set src rect info
          src_rect.x = ALIGN_DOWN((int)drmLayer->source_crop.left,2);
          src_rect.y = ALIGN_DOWN((int)drmLayer->source_crop.top,2);
          src_rect.width  = ALIGN_DOWN((int)(drmLayer->source_crop.right  - drmLayer->source_crop.left),2);
          src_rect.height = ALIGN_DOWN((int)(drmLayer->source_crop.bottom - drmLayer->source_crop.top),2);

          // Set dst buffer info
          dst.fd      = dst_buffer->GetFd();
          dst.width   = dst_buffer->GetWidth();
          dst.height  = dst_buffer->GetHeight();
          // RGA 的特殊修改，需要通过 wstride
          if(dst_buffer->GetFourccFormat() == DRM_FORMAT_NV15)
            dst.wstride = dst_buffer->GetByteStride();
          else
            dst.wstride = dst_buffer->GetStride();

          dst.hstride = dst_buffer->GetHeightStride();
          dst.format  = dst_buffer->GetFormat();

          // 若缩放倍数超出RGA最大缩小倍数，则进行二次缩放，倍率设置为4
          if(rga_scale_max){
            int scale_max_rate = 4;

            // Set dst rect info
            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.width  = ALIGN_DOWN((int)(drmLayer->source_crop.right
                                                - drmLayer->source_crop.left) / scale_max_rate,2);
            dst_rect.height = ALIGN_DOWN((int)(drmLayer->source_crop.bottom
                                                - drmLayer->source_crop.top) / scale_max_rate,2);
          }else{
            // Set dst rect info
            dst_rect.x = 0;
            dst_rect.y = 0;
            dst_rect.width  = ALIGN_DOWN((int)(drmLayer->display_frame.right  - drmLayer->display_frame.left),2);
            dst_rect.height = ALIGN_DOWN((int)(drmLayer->display_frame.bottom - drmLayer->display_frame.top),2);
          }

          // 处理旋转
          switch(drmLayer->transform){
          case DRM_MODE_ROTATE_0:
            usage = 0;
            break;
          case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X:
            usage = IM_HAL_TRANSFORM_FLIP_H;
            break;
          case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y:
            usage = IM_HAL_TRANSFORM_FLIP_V;
            break;
          case DRM_MODE_ROTATE_90:
            usage = IM_HAL_TRANSFORM_ROT_90;
            break;
          case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y:
            usage = IM_HAL_TRANSFORM_ROT_180;
            break;
          case DRM_MODE_ROTATE_270:
            usage = IM_HAL_TRANSFORM_ROT_270;
            break;
          // RGA2/RGA3的 flip + rotate 场景，硬件内部处理是先 rotate 再 flip
          // 而 Android 请求的是先 flip 再 rotate，故此请求需要做转换
          // Android请求 flip-v + rotate-90  等价于 rotate-90 + flip-h
          case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_Y | DRM_MODE_ROTATE_90 :
            usage = IM_HAL_TRANSFORM_ROT_90 | IM_HAL_TRANSFORM_FLIP_H ;
            break;
          // Android请求 flip-h + rotate-90  等价于 rotate-90 + flip-v
          case DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_ROTATE_90:
            usage = IM_HAL_TRANSFORM_ROT_90 | IM_HAL_TRANSFORM_FLIP_V;
            break;
          default:
            usage = 0;
            ALOGE_IF(LogLevel(DBG_DEBUG),"Unknow sf transform 0x%x", drmLayer->transform);
          }

          IM_STATUS im_state;
          // Call Im2d 格式转换
          im_state = imcheck_composite(src, dst, pat, src_rect, dst_rect, pat_rect, usage | IM_ASYNC);
          if(im_state != IM_STATUS_NOERROR){
            HWC2_ALOGE("call im2d scale fail, %s",imStrError(im_state));
            break;
          }

          hwc_frect_t source_crop;
          source_crop.left   = dst_rect.x;
          source_crop.top    = dst_rect.y;
          source_crop.right  = dst_rect.x + dst_rect.width;
          source_crop.bottom = dst_rect.y + dst_rect.height;
          drmLayer->UpdateAndStoreInfoFromDrmBuffer(dst_buffer->GetHandle(),
                                                    dst_buffer->GetFd(),
                                                    dst_buffer->GetFormat(),
                                                    dst_buffer->GetWidth(),
                                                    dst_buffer->GetHeight(),
                                                    dst_buffer->GetStride(),
                                                    dst_buffer->GetHeightStride(),
                                                    dst_buffer->GetByteStride(),
                                                    dst_buffer->GetSize(),
                                                    dst_buffer->GetUsage(),
                                                    dst_buffer->GetFourccFormat(),
                                                    dst_buffer->GetModifier(),
                                                    dst_buffer->GetByteStridePlanes(),
                                                    dst_buffer->GetName(),
                                                    source_crop,
                                                    dst_buffer->GetBufferId(),
                                                    dst_buffer->GetGemHandle(),
                                                    DRM_MODE_ROTATE_0);
          rga_layer_ready = true;
          drmLayer->iBestPlaneType = DRM_PLANE_TYPE_VOP1_WIN0|DRM_PLANE_TYPE_VOP0_WIN0|DRM_PLANE_TYPE_VOP0_WIN1;
          drmLayer->pRgaBuffer_ = dst_buffer;
          drmLayer->bUseRga_ = true;
          break;
        }else{
          dst_buffer = rgaBufferQueue_->BackDrmBuffer();

          if(dst_buffer == NULL){
            HWC2_ALOGD_IF_DEBUG("DequeueDrmBuffer fail!, skip this policy.");
            break;
          }

          hwc_frect_t source_crop;
          source_crop.left  = 0;
          source_crop.top   = 0;
          source_crop.right =   ALIGN_DOWN((int)(drmLayer->display_frame.right  - drmLayer->display_frame.left),2);
          source_crop.bottom  = ALIGN_DOWN((int)(drmLayer->display_frame.bottom - drmLayer->display_frame.top),2);
          drmLayer->UpdateAndStoreInfoFromDrmBuffer(dst_buffer->GetHandle(),
                                                    dst_buffer->GetFd(),
                                                    dst_buffer->GetFormat(),
                                                    dst_buffer->GetWidth(),
                                                    dst_buffer->GetHeight(),
                                                    dst_buffer->GetStride(),
                                                    dst_buffer->GetHeightStride(),
                                                    dst_buffer->GetByteStride(),
                                                    dst_buffer->GetSize(),
                                                    dst_buffer->GetUsage(),
                                                    dst_buffer->GetFourccFormat(),
                                                    dst_buffer->GetModifier(),
                                                    dst_buffer->GetByteStridePlanes(),
                                                    dst_buffer->GetName(),
                                                    source_crop,
                                                    dst_buffer->GetBufferId(),
                                                    dst_buffer->GetGemHandle(),
                                                    DRM_MODE_ROTATE_0);
          use_laster_rga_layer = true;
          drmLayer->bUseRga_ = true;
          drmLayer->iBestPlaneType = DRM_PLANE_TYPE_VOP1_WIN0|DRM_PLANE_TYPE_VOP0_WIN0|DRM_PLANE_TYPE_VOP0_WIN1;
          drmLayer->pRgaBuffer_ = dst_buffer;
          break;
        }
      }
  }
  if(rga_layer_ready){
    ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d rga layer ready, to matchPlanes",__FUNCTION__,__LINE__);
    int ret = 0;
    if(ctx.request.iSkipCnt > 0){
      ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    }else{
      ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
      if(ret){
        ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
      }
    }
    if(!ret){ // Match sucess, to call im2d interface
      for(auto &drmLayer : layers){
        if(drmLayer->bUseRga_){
          im_opt_t imOpt;
          memset(&imOpt, 0x00, sizeof(im_opt_t));
          imOpt.core = IM_SCHEDULER_RGA2_CORE0;

          IM_STATUS im_state = improcess(src, dst, pat, src_rect, dst_rect, pat_rect, 0, &releaseFence, &imOpt, usage | IM_ASYNC);
          if(im_state != IM_STATUS_SUCCESS){
            HWC2_ALOGE("call im2d scale fail, %s",imStrError(im_state));
            rgaBufferQueue_->QueueBuffer(dst_buffer);
            drmLayer->ResetInfoFromStore();
            drmLayer->bUseRga_ = false;
            ret = -1;
            break;
          }
          dst_buffer->SetFinishFence(dup(releaseFence));
          drmLayer->pRgaBuffer_ = dst_buffer;
          drmLayer->acquire_fence = sp<AcquireFence>(new AcquireFence(releaseFence));
          rgaBufferQueue_->QueueBuffer(dst_buffer);
          last_buffer_id = drmLayer->uBufferId_;
          return ret;
        }
      }
      ResetLayerFromTmp(layers,tmp_layers);
      return ret;
    }else{ // Match fail, skip rga policy
      HWC2_ALOGD_IF_DEBUG(" MatchPlanes fail! reset DrmHwcLayer.");
      for(auto &drmLayer : layers){
        if(drmLayer->bUseRga_){
          rgaBufferQueue_->QueueBuffer(dst_buffer);
          drmLayer->ResetInfoFromStore();
          drmLayer->bUseRga_ = false;
        }
      }
      ResetLayerFromTmp(layers,tmp_layers);
      return -1;
    }
  }else if(use_laster_rga_layer){
    ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d rga layer ready, to matchPlanes",__FUNCTION__,__LINE__);
    int ret = -1;
    if(ctx.request.iSkipCnt > 0){
      ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    }else{
      ret = TryOverlayPolicy(composition,layers,crtc,plane_groups);
      if(ret){
        ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
      }
    }
    if(!ret){ // Match sucess, to call im2d interface
      HWC2_ALOGD_IF_DEBUG("Use last rga layer.");
      return ret;
    }
  }
  HWC2_ALOGD_IF_DEBUG("fail!, No layer use RGA policy.");
  ResetLayerFromTmp(layers,tmp_layers);
#endif
  return -1;
}

int Vop3399::TryMixSkipPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);

  int skipCnt = 0;

  int iPlaneSize = plane_groups.size();

  if(iPlaneSize == 0){
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, iPlaneSize = %d, skip TryMixSkipPolicy",
              __FUNCTION__,__LINE__,iPlaneSize);
  }

  std::vector<DrmHwcLayer *> tmp_layers;
  // Since we can't composite HWC_SKIP_LAYERs by ourselves, we'll let SF
  // handle all layers in between the first and last skip layers. So find the
  // outer indices and mark everything in between as HWC_FRAMEBUFFER
  std::pair<int, int> skip_layer_indices(-1, -1);

  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  //caculate the first and last skip layer
  int i = 0;
  for (auto &layer : layers) {
    if (!layer->bSkipLayer_ && !layer->bGlesCompose_){
      i++;
      continue;
    }

    if (skip_layer_indices.first == -1)
      skip_layer_indices.first = i;
    skip_layer_indices.second = i;
    i++;
  }

  if(skip_layer_indices.first != -1){
    skipCnt = skip_layer_indices.second - skip_layer_indices.first + 1;
  }else{
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, can't find any skip layer, first = %d, second = %d",
              __FUNCTION__,__LINE__,skip_layer_indices.first,skip_layer_indices.second);
    return -1;
  }

  HWC2_ALOGD_IF_DEBUG("mix skip (%d,%d)",skip_layer_indices.first, skip_layer_indices.second);
  OutputMatchLayer(skip_layer_indices.first, skip_layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret){
    return ret;
  }else{
    ResetLayerFromTmp(layers,tmp_layers);
    //save fb into tmp_layers
    MoveFbToTmp(layers, tmp_layers);
    int first = skip_layer_indices.first;
    int last = skip_layer_indices.second;
    // 建议zpos大的图层走GPU合成
    for(last++; last < layers.size() - 1; last++){
      HWC2_ALOGD_IF_DEBUG("mix skip (%d,%d)",first, last);
      OutputMatchLayer(first, last, layers, tmp_layers);
      ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(ret){
        ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,first, last);
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }else{
        return ret;
      }
    }
    last = layers.size() - 1;
    // 逐步建议知道zpos=0走GPU合成，即全GPU合成
    for(; first >= 0; first--){
      HWC2_ALOGD_IF_DEBUG("mix skip (%d,%d)",first, last);
      OutputMatchLayer(first, last, layers, tmp_layers);
      ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(ret){
        ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d fail match (%d,%d)",__FUNCTION__,__LINE__,first, last);
        ResetLayerFromTmpExceptFB(layers,tmp_layers);
        continue;
      }else{
        return ret;
      }
    }
  }
  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix video*************************
 Video ovelay
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
      GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
      GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
      GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
      GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
************************************************************/
int Vop3399::TryMixVideoPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ResetLayerFromTmpExceptFB(layers,tmp_layers);
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmp(layers,tmp_layers);
     }
   }
 }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix up*************************
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
       HWC | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
       HWC | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
       HWC | 711aa61700 | 0000 | 0000 | 00 | 0100 | ? 00000017  |    0.0,    0.0, 3840.0, 2160.0 |  600,  562, 1160,  982 | SurfaceView - MediaView
      GLES | 711ab1e580 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,  560.0,  420.0 |  600,  562, 1160,  982 | MediaView
      GLES | 70b34c9c80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
      GLES | 70b34c9080 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
      GLES | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 | 1136, 1194, 1175, 1243 | Sprite
************************************************************/
int Vop3399::TryMixUpPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;
  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  int iPlaneSize = plane_groups.size();

  if(iPlaneSize == 0){
    ALOGE_IF(LogLevel(DBG_DEBUG), "%s:line=%d, iPlaneSize = %d, skip TryMixSkipPolicy",
              __FUNCTION__,__LINE__,iPlaneSize);
  }

  std::pair<int, int> layer_indices(-1, -1);

  if((int)layers.size() < 4)
    layer_indices.first = layers.size() - 2 <= 0 ? 1 : layers.size() - 2;
  else
    layer_indices.first = 3;
  layer_indices.second = layers.size() - 1;
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
  OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
  int ret = MatchPlanes(composition,layers,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmpExceptFB(layers,tmp_layers);
    for(--layer_indices.first; layer_indices.first > 0; --layer_indices.first){
      ResetLayerFromTmpExceptFB(layers,tmp_layers);
      ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix video (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
      OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
      ret = MatchPlanes(composition,layers,crtc,plane_groups);
      if(!ret)
        return ret;
      else{
        ResetLayerFromTmp(layers,tmp_layers);
        return -1;
     }
   }
 }

  ResetLayerFromTmp(layers,tmp_layers);
  return ret;
}

/*************************mix down*************************
 Sprite layer
-----------+----------+------+------+----+------+-------------+--------------------------------+------------------------+------
      GLES | 711aa61e80 | 0000 | 0000 | 00 | 0100 | RGBx_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.systemui.ImageWallpaper
      GLES | 711ab1ef00 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0, 1600.0 |    0,    0, 2400, 1600 | com.android.launcher3/com.android.launcher3.Launcher
      GLES | 711aa61100 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,    2.0 |    0,    0, 2400,    2 | StatusBar
       HWC | 711ec5ad80 | 0000 | 0000 | 00 | 0105 | RGBA_8888   |    0.0,    0.0, 2400.0,   84.0 |    0, 1516, 2400, 1600 | taskbar
       HWC | 711ec5a900 | 0000 | 0002 | 00 | 0105 | RGBA_8888   |    0.0,    0.0,   39.0,   49.0 |  941,  810,  980,  859 | Sprite
************************************************************/
int Vop3399::TryMixDownPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  std::vector<DrmHwcLayer *> tmp_layers;

  //save fb into tmp_layers
  MoveFbToTmp(layers, tmp_layers);

  if(layers.size() < 4 || layers.size() > 6 ){
    ResetLayerFromTmp(layers,tmp_layers);
    return -1;
  }

  std::pair<int, int> layer_indices(-1, -1);
  int iPlaneSize = 0;
  for(auto plane_group:plane_groups)
    iPlaneSize+=plane_group->planes.size();
  layer_indices.first = 0;
  if(layers.size()>iPlaneSize)
    layer_indices.second = layers.size()-iPlaneSize;
  else
    layer_indices.second = 0;
  while(layer_indices.second<layers.size()-2){
    ALOGD_IF(LogLevel(DBG_DEBUG), "%s:mix down (%d,%d)",__FUNCTION__,layer_indices.first, layer_indices.second);
    OutputMatchLayer(layer_indices.first, layer_indices.second, layers, tmp_layers);
    int ret = MatchPlanes(composition,layers,crtc,plane_groups);
    if(!ret)
      return ret;
    else
      ResetLayerFromTmpExceptFB(layers,tmp_layers);
    layer_indices.second++;
  }

  ResetLayerFromTmp(layers,tmp_layers);
  return -1;
}

int Vop3399::TryMixPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  int ret;
  if(ctx.state.setHwcPolicy.count(HWC_MIX_SKIP_POLICY)){
    ret = TryMixSkipPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
    else
      return ret;
  }

  if(ctx.state.setHwcPolicy.count(HWC_MIX_VIDEO_POLICY)){
    ret = TryMixVideoPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }

  if(ctx.state.setHwcPolicy.count(HWC_RGA_OVERLAY_POLICY)){
    ret = TryRgaOverlayPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }

  if(ctx.state.setHwcPolicy.count(HWC_MIX_UP_POLICY)){
    ret = TryMixUpPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;

  }
  if(ctx.state.setHwcPolicy.count(HWC_MIX_DOWN_POLICY)){
    ret = TryMixDownPolicy(composition,layers,crtc,plane_groups);
    if(!ret)
      return 0;
  }
  return -1;
}

int Vop3399::TryGLESPolicy(
    std::vector<DrmCompositionPlane> *composition,
    std::vector<DrmHwcLayer*> &layers, DrmCrtc *crtc,
    std::vector<PlaneGroup *> &plane_groups) {
  ALOGD_IF(LogLevel(DBG_DEBUG), "%s:line=%d",__FUNCTION__,__LINE__);
  std::vector<DrmHwcLayer*> fb_target;
  ResetLayer(layers);
  ResetPlaneGroups(plane_groups);
  //save fb into tmp_layers
  MoveFbToTmp(layers, fb_target);

  if(fb_target.size()==1){
    DrmHwcLayer* fb_layer = fb_target[0];
    // If there is a Cluster layer, FB enables AFBC
    if(ctx.support.iAfbcdCnt > 0){
      ctx.state.bDisableFBAfbcd = false;

      // Check FB target property
      ctx.state.bDisableFBAfbcd = hwc_get_int_property("vendor.gralloc.no_afbc_for_fb_target_layer","0") > 0;

      // If FB-target unable to meet the scaling requirements, AFBC must be disable.
      if((fb_layer->fHScaleMul_ > 8.0 || fb_layer->fHScaleMul_ < 0.125) ||
         (fb_layer->fVScaleMul_ > 8.0 || fb_layer->fVScaleMul_ < 0.125) ){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d FB-target over max scale factor, FB-target must disable AFBC(%d).",
             __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }
      if(ctx.state.bDisableFBAfbcd){
        fb_layer->bAfbcd_ = false;
      }else{
        fb_layer->bAfbcd_ = true;
        ALOGD_IF(LogLevel(DBG_DEBUG),"%s,line=%d FB enables AFBC",__FUNCTION__,__LINE__);
      }
    }
  }
  int ret = MatchPlanes(composition,fb_target,crtc,plane_groups);
  if(!ret)
    return ret;
  else{
    ResetLayerFromTmp(layers,fb_target);
    return -1;
  }
  return 0;
}

void Vop3399::UpdateResevedPlane(DrmCrtc *crtc){
  // Reserved DrmPlane
  char reserved_plane_name[PROPERTY_VALUE_MAX] = {0};
  hwc_get_string_property("vendor.hwc.reserved_plane_name","NULL",reserved_plane_name);

  if(strlen(ctx.support.arrayReservedPlaneName) == 0 ||
     strcmp(reserved_plane_name,ctx.support.arrayReservedPlaneName)){
    int reserved_plane_win_type = 0;
    strncpy(ctx.support.arrayReservedPlaneName,reserved_plane_name,strlen(reserved_plane_name)+1);
    DrmDevice *drm = crtc->getDrmDevice();
    std::vector<PlaneGroup *> all_plane_groups = drm->GetPlaneGroups();
    for(auto &plane_group : all_plane_groups){
      for(auto &p : plane_group->planes){
        if(!strcmp(p->name(),ctx.support.arrayReservedPlaneName)){
          plane_group->bReserved = true;
          reserved_plane_win_type = plane_group->win_type;
          ALOGI("%s,line=%d Reserved DrmPlane %s , win_type = 0x%x",
            __FUNCTION__,__LINE__,ctx.support.arrayReservedPlaneName,reserved_plane_win_type);
          break;
        }else{
          plane_group->bReserved = false;
        }
      }
    }
    // RK3566 must reserved a extra DrmPlane.
    if(ctx.state.iSocId == 0x3566 || ctx.state.iSocId == 0x3566a){
      switch(reserved_plane_win_type){
        case DRM_PLANE_TYPE_CLUSTER0_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_CLUSTER1_WIN0;
          break;
        case DRM_PLANE_TYPE_CLUSTER0_WIN1:
          reserved_plane_win_type |= DRM_PLANE_TYPE_CLUSTER0_WIN0;
          break;
        case DRM_PLANE_TYPE_ESMART0_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_ESMART1_WIN0;
          break;
        case DRM_PLANE_TYPE_ESMART1_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_ESMART0_WIN0;
          break;
        case DRM_PLANE_TYPE_SMART0_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_SMART1_WIN0;
          break;
        case DRM_PLANE_TYPE_SMART1_WIN0:
          reserved_plane_win_type |= DRM_PLANE_TYPE_SMART0_WIN0;
          break;
        default:
          reserved_plane_win_type = 0;
          break;
      }
      for(auto &plane_group : all_plane_groups){
        if(reserved_plane_win_type & plane_group->win_type){
          plane_group->bReserved = true;
          ALOGI_IF(1 || LogLevel(DBG_DEBUG),"%s,line=%d Reserved win_type = 0x%x",
            __FUNCTION__,__LINE__,reserved_plane_win_type);
          break;
        }else{
          plane_group->bReserved = false;
        }
      }
    }
  }
  return;
}

/*
 * CLUSTER_AFBC_DECODE_MAX_RATE = 3.2
 * (src(W*H)/dst(W*H))/(aclk/dclk) > CLUSTER_AFBC_DECODE_MAX_RATE to use GLES compose.
 * Notes: (4096,1714)=>(1080,603) appear( DDR 1560M ), CLUSTER_AFBC_DECODE_MAX_RATE=2.839350
 * Notes: (4096,1714)=>(1200,900) appear( DDR 1056M ), CLUSTER_AFBC_DECODE_MAX_RATE=2.075307
 */
#define CLUSTER_AFBC_DECODE_MAX_RATE 2.0
bool Vop3399::CheckGLESLayer(DrmHwcLayer *layer){
  // RK356x can't overlay RGBA1010102
  if(layer->iFormat_ == HAL_PIXEL_FORMAT_RGBA_1010102){
    HWC2_ALOGD_IF_DEBUG("[%s]：RGBA1010102 format, not support overlay.",
              layer->sLayerName_.c_str());
    return true;
  }


  int act_w = static_cast<int>(layer->source_crop.right - layer->source_crop.left);
  int act_h = static_cast<int>(layer->source_crop.bottom - layer->source_crop.top);
  int dst_w = static_cast<int>(layer->display_frame.right - layer->display_frame.left);
  int dst_h = static_cast<int>(layer->display_frame.bottom - layer->display_frame.top);

  // RK platform VOP can't display src/dst w/h < 4 layer.
  if(act_w < 4 || act_h < 4 || dst_w < 4 || dst_h < 4){
    HWC2_ALOGD_IF_DEBUG("[%s]：[%dx%d] => [%dx%d] too small, use GLES composer.",
              layer->sLayerName_.c_str(),act_w,act_h,dst_w,dst_h);
    return true;
  }

  //TODO::AFBC limit for RK3399

  if(layer->bAfbcd_){
    bool disable_afbc = false;
    if(layer->source_crop.left!=0 || layer->source_crop.top!=0){
      HWC2_ALOGD_IF_DEBUG("[%s]source_crop =[%d,%d,%d,%d] not support offset.", 
                          layer->sLayerName_.c_str(), 
                          (int)layer->source_crop.left, (int)layer->source_crop.top,
                          (int)layer->source_crop.right, (int)layer->source_crop.bottom);
      disable_afbc = true;
    }
    if(act_w>2560){
      HWC2_ALOGD_IF_DEBUG("[%s]act_w =%d too big, maximum 2560", 
                          layer->sLayerName_.c_str(), act_w);
      disable_afbc = true;
    }
    if(!layer->bFbTarget_){
      if((layer->iStride_&(16-1)) || (layer->iHeightStride_&(8-1))){
        HWC2_ALOGD_IF_DEBUG("[%s]stride =[%d,%d] not aligned to [16x8].", 
                            layer->sLayerName_.c_str(), layer->iStride_, layer->iHeightStride_);
        disable_afbc = true;
      }
    }
    if(disable_afbc){
      if(layer->bFbTarget_){
        HWC2_ALOGD_IF_DEBUG("[%s] FB target do not meet AFBC limit, disable AFBC.", 
                            layer->sLayerName_.c_str());
        layer->bAfbcd_ = false;
      }
      return true;
    }
  }

  if(layer->transform == -1){
    HWC2_ALOGD_IF_DEBUG("[%s]：transform unknow, use GLES",
            layer->sLayerName_.c_str());
    return true;
  }

  switch(layer->sf_composition){
    case HWC2::Composition::Client:
    case HWC2::Composition::Sideband:
    case HWC2::Composition::SolidColor:
      HWC2_ALOGD_IF_DEBUG("[%s]：sf_composition =0x%x not support overlay.",
              layer->sLayerName_.c_str(),layer->sf_composition);
      return true;
    default:
      break;
  }
  return false;
}

void Vop3399::InitRequestContext(std::vector<DrmHwcLayer*> &layers){

  // Collect layer info
  ctx.request.iAfbcdCnt=0;
  ctx.request.iAfbcdScaleCnt=0;
  ctx.request.iAfbcdYuvCnt=0;
  ctx.request.iAfcbdLargeYuvCnt=0;
  ctx.request.iAfbcdRotateCnt=0;
  ctx.request.iAfbcdHdrCnt=0;

  ctx.request.iScaleCnt=0;
  ctx.request.iYuvCnt=0;
  ctx.request.iLargeYuvCnt=0;
  ctx.request.iSkipCnt=0;
  ctx.request.iRotateCnt=0;
  ctx.request.iHdrCnt=0;

  for(auto &layer : layers){
    if(CheckGLESLayer(layer)){
      layer->bGlesCompose_ = true;
    }else{
      layer->bGlesCompose_ = false;
    }

    if(layer->bFbTarget_)
      continue;

    if(layer->bSkipLayer_ || layer->bGlesCompose_){
      ctx.request.iSkipCnt++;
      continue;
    }

    if(layer->bAfbcd_){
      ctx.request.iAfbcdCnt++;

      if(layer->bScale_)
        ctx.request.iAfbcdScaleCnt++;

      if(layer->bYuv_){
        ctx.request.iAfbcdYuvCnt++;
        int dst_w = static_cast<int>(layer->display_frame.right - layer->display_frame.left);
        if(layer->iWidth_ > 2048 || layer->bHdr_ || dst_w > 2048){
          ctx.request.iAfcbdLargeYuvCnt++;
        }
      }

      if(layer->transform != DRM_MODE_ROTATE_0)
        ctx.request.iAfbcdRotateCnt++;

      if(layer->bHdr_)
        ctx.request.iAfbcdHdrCnt++;

    }else{

      ctx.request.iCnt++;

      if(layer->bScale_)
        ctx.request.iScaleCnt++;

      if(layer->bYuv_){
        ctx.request.iYuvCnt++;
        if(layer->iWidth_ > 2048){
          ctx.request.iLargeYuvCnt++;
        }
      }

      if(layer->transform != DRM_MODE_ROTATE_0)
        ctx.request.iRotateCnt++;

      if(layer->bHdr_)
        ctx.request.iHdrCnt++;
    }
  }
  return;
}

void Vop3399::InitSupportContext(
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc){
  // Collect Plane resource info
  ctx.support.iAfbcdCnt=0;
  ctx.support.iAfbcdScaleCnt=0;
  ctx.support.iAfbcdYuvCnt=0;
  ctx.support.iAfbcdRotateCnt=0;
  ctx.support.iAfbcdHdrCnt=0;

  ctx.support.iCnt=0;
  ctx.support.iScaleCnt=0;
  ctx.support.iYuvCnt=0;
  ctx.support.iRotateCnt=0;
  ctx.support.iHdrCnt=0;

  // Update DrmPlane
  UpdateResevedPlane(crtc);

  for(auto &plane_group : plane_groups){
    if(plane_group->bReserved)
      continue;
    for(auto &p : plane_group->planes){
      if(p->get_afbc()){

        ctx.support.iAfbcdCnt++;

        if(p->get_scale()){
          ctx.support.iAfbcdScaleCnt++;
          ctx.support.iScaleCnt++;
        }

        if(p->get_rotate()){
          ctx.support.iAfbcdRotateCnt++;
          ctx.support.iRotateCnt++;
        }

        if(p->get_hdr2sdr())
          ctx.support.iAfbcdHdrCnt++;

        if(p->get_yuv())
          ctx.support.iYuvCnt++;

      }else{

        ctx.support.iCnt++;

        if(p->get_scale())
          ctx.support.iScaleCnt++;

        if(p->get_yuv())
          ctx.support.iYuvCnt++;

        if(p->get_rotate())
          ctx.support.iRotateCnt++;

        if(p->get_hdr2sdr())
          ctx.support.iHdrCnt++;
      }
    }
  }
  return;
}

void Vop3399::InitStateContext(
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc){
  ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d bMultiAreaEnable=%d, bRgaPolicyEnable=%d",
            __FUNCTION__,__LINE__,ctx.state.bMultiAreaEnable,ctx.state.bRgaPolicyEnable);

  // FB-target need disable AFBCD?
  ctx.state.bDisableFBAfbcd = false;
  for(auto &layer : layers){
    if(layer->bFbTarget_){
      if(ctx.support.iAfbcdCnt == 0){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d VOP LIT, FB-target must disable AFBC(%d).",
            __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }

      if(ctx.request.iAfcbdLargeYuvCnt > 0 && ctx.support.iAfbcdYuvCnt <= 2){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d FIXME:request.AfcbdLargeYuvCnt>0,support.iAfbcdYuvCnt <= 2 FB-target must disable AFBC(%d).",
            __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }

      // If FB-target unable to meet the scaling requirements, AFBC must be disable.
      if((layer->fHScaleMul_ > 8.0 || layer->fHScaleMul_ < 0.125) ||
         (layer->fVScaleMul_ > 8.0 || layer->fVScaleMul_ < 0.125) ){
        ctx.state.bDisableFBAfbcd = true;
        ALOGI_IF(LogLevel(DBG_DEBUG),"%s,line=%d FB-target over max scale factor, FB-target must disable AFBC(%d).",
             __FUNCTION__,__LINE__,ctx.state.bDisableFBAfbcd);
      }

      if(ctx.state.bDisableFBAfbcd){
        layer->bAfbcd_ = 0;
      }
      break;
    }
  }

  // Check dispaly Mode : save width and height.
  DrmDevice *drm = crtc->getDrmDevice();
  DrmConnector *conn = drm->GetConnectorForDisplay(crtc->display());
  if(conn && conn->state() == DRM_MODE_CONNECTED){
    DrmMode mode = conn->current_mode();
    // Story Display Mode
    ctx.state.iDisplayWidth_ = mode.h_display();
    ctx.state.iDisplayHeight_ = mode.v_display();
  }

  return;
}


bool Vop3399::TryOverlay(){
  if(ctx.request.iAfbcdCnt <= ctx.support.iAfbcdCnt &&
     ctx.request.iScaleCnt <= ctx.support.iScaleCnt &&
     ctx.request.iYuvCnt <= ctx.support.iYuvCnt &&
     ctx.request.iRotateCnt <= ctx.support.iRotateCnt &&
     ctx.request.iSkipCnt == 0){
    ctx.state.setHwcPolicy.insert(HWC_OVERLAY_POLICY);
    return true;
  }
  return false;
}

void Vop3399::TryMix(){
  ctx.state.setHwcPolicy.insert(HWC_MIX_POLICY);
  ctx.state.setHwcPolicy.insert(HWC_MIX_UP_POLICY);
  ctx.state.setHwcPolicy.insert(HWC_MIX_DOWN_POLICY);
  if(ctx.support.iYuvCnt > 0 || ctx.support.iAfbcdYuvCnt > 0){
    ctx.state.setHwcPolicy.insert(HWC_RGA_OVERLAY_POLICY);
    ctx.state.setHwcPolicy.insert(HWC_MIX_VIDEO_POLICY);
  }
  if(ctx.request.iSkipCnt > 0)
    ctx.state.setHwcPolicy.insert(HWC_MIX_SKIP_POLICY);
}

int Vop3399::InitContext(
    std::vector<DrmHwcLayer*> &layers,
    std::vector<PlaneGroup *> &plane_groups,
    DrmCrtc *crtc,
    bool gles_policy){

  ctx.state.setHwcPolicy.clear();
  ctx.state.iSocId = crtc->get_soc_id();

  InitRequestContext(layers);
  InitSupportContext(plane_groups,crtc);
  InitStateContext(layers,plane_groups,crtc);

  //force go into GPU
  int iMode = hwc_get_int_property("vendor.hwc.compose_policy","0");

  if(((iMode!=1 && iMode!=6) || gles_policy) && iMode != 2){
    ctx.state.setHwcPolicy.insert(HWC_GLES_POLICY);
    ALOGD_IF(LogLevel(DBG_DEBUG),"Force use GLES compose, iMode=%d, gles_policy=%d, soc_id=%x",iMode,gles_policy,ctx.state.iSocId);
    return 0;
  }

  ALOGD_IF(LogLevel(DBG_DEBUG),"request:afbcd=%d,scale=%d,yuv=%d,rotate=%d,hdr=%d,skip=%d\n"
          "support:afbcd=%d,scale=%d,yuv=%d,rotate=%d,hdr=%d, %s,line=%d,",
          ctx.request.iAfbcdCnt,ctx.request.iScaleCnt,ctx.request.iYuvCnt,
          ctx.request.iRotateCnt,ctx.request.iHdrCnt,ctx.request.iSkipCnt,
          ctx.support.iAfbcdCnt,ctx.support.iScaleCnt,ctx.support.iYuvCnt,
          ctx.support.iRotateCnt,ctx.support.iHdrCnt,
          __FUNCTION__,__LINE__);

  // Match policy first
  if(!TryOverlay())
    TryMix();

  return 0;
}
}

