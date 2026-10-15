// :bustub-keep-private:
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer.cpp
//
// Identification: src/buffer/arc_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/arc_replacer.h"
#include <optional>
#include "common/config.h"
#include <algorithm>

namespace bustub {

/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new ArcReplacer, with lists initialized to be empty and target size to 0
 * @param num_frames the maximum number of frames the ArcReplacer will be required to cache
 */
ArcReplacer::ArcReplacer(size_t num_frames) : replacer_size_(num_frames) {}

/**
 * TODO(P1): Add implementation
 *
 * @brief Performs the Replace operation as described by the writeup
 * that evicts from either mfu_ or mru_ into its corresponding ghost list
 * according to balancing policy.
 *
 * If you wish to refer to the original ARC paper, please note that there are
 * two changes in our implementation:
 * 1. When the size of mru_ equals the target size, we don't check
 * the last access as the paper did when deciding which list to evict from.
 * This is fine since the original decision is stated to be arbitrary.
 * 2. Entries that are not evictable are skipped. If all entries from the desired side
 * (mru_ / mfu_) are pinned, we instead try victimize the other side (mfu_ / mru_),
 * and move it to its corresponding ghost list (mfu_ghost_ / mru_ghost_).
 *
 * @return frame id of the evicted frame, or std::nullopt if cannot evict
 */
auto ArcReplacer::Evict() -> std::optional<frame_id_t> { 
  std::scoped_lock lock(latch_);
  if(curr_size_==0){
    return std::nullopt;
  }
  bool prefer_mru = mru_.size()>=mru_target_size_;
  for(int round = 0; round < 2; round++){
    bool use_mru=(round == 0)? prefer_mru: !prefer_mru;

    auto &alive_list = use_mru? mru_ : mfu_;
    auto &ghost_list = use_mru? mru_ghost_: mfu_ghost_;

    for(auto it = alive_list.rbegin(); it!=alive_list.rend();++it)
    {
      auto alive_it=alive_map_.find(*it);
      auto status=alive_it->second;
      if(!status->evictable_){
        continue;
      }

      frame_id_t victim_frame_id = status->frame_id_;
      page_id_t victim_page_id = status->page_id_;
      alive_list.erase(status->alive_pos_);
      alive_map_.erase(alive_it);

      ghost_list.push_front(victim_page_id);
      status->ghost_pos_=ghost_list.begin();

      status->arc_status_=use_mru? ArcStatus::MRU_GHOST : ArcStatus::MFU_GHOST;
      status->frame_id_=-1;
      status->evictable_=false;
      ghost_map_.emplace(victim_page_id,status);

      curr_size_--;
      return victim_frame_id;
    }
  }
  return std::nullopt; 
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Record access to a frame, adjusting ARC bookkeeping accordingly
 * by bring the accessed page to the front of mfu_ if it exists in any of the lists
 * or the front of mru_ if it does not.
 *
 * Performs the operations EXCEPT REPLACE described in original paper, which is
 * handled by `Evict()`.
 *
 * Consider the following four cases, handle accordingly:
 * 1. Access hits mru_ or mfu_
 * 2/3. Access hits mru_ghost_ / mfu_ghost_
 * 4. Access misses all the lists
 *
 * This routine performs all changes to the four lists as preperation
 * for `Evict()` to simply find and evict a victim into ghost lists.
 *
 * Note that frame_id is used as identifier for alive pages and
 * page_id is used as identifier for the ghost pages, since page_id is
 * the unique identifier to the page after it's dead.
 * Using page_id for alive pages should be the same since it's one to one mapping,
 * but using frame_id is slightly more intuitive.
 *
 * @param frame_id id of frame that received a new access.
 * @param page_id id of page that is mapped to the frame.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, [[maybe_unused]] AccessType access_type) {
  std::scoped_lock lock(latch_);
  BUSTUB_ENSURE(
    frame_id >= 0 && static_cast<size_t>(frame_id) <= replacer_size_,
    "invalid frame id");//frame号不在范围，报错
    auto it=alive_map_.find(frame_id);//内存存在，把它扔到mfu头部
    if(it!=alive_map_.end()){
      auto &status =*it->second;
      if(status.arc_status_== ArcStatus::MRU){
        mfu_.splice(mfu_.begin(),mru_,status.alive_pos_);
      }else{
        mfu_.splice(mfu_.begin(),mfu_,status.alive_pos_);
      }

      status.arc_status_ =ArcStatus::MFU;
      return ;
    }
    auto ghost_it = ghost_map_.find(page_id);
    if(ghost_it == ghost_map_.end()){
      if(mru_.size()+mru_ghost_.size()==replacer_size_){
        auto old_page_id = mru_ghost_.back();
        ghost_map_.erase(old_page_id);
        mru_ghost_.pop_back();//mru+mru_ghost==c,那么得把mru_ghost踢一个
      }else if(mru_.size()+mfu_.size()+mru_ghost_.size()+mfu_ghost_.size()== 2*replacer_size_){
        auto old_page_id = mfu_ghost_.back();
        ghost_map_.erase(old_page_id);
        mfu_ghost_.pop_back(); //走到 else if，意味着第一个条件不成立。按照算法维护的约束，所以把新页面加入 MRU，不会突破这一组的上限。但四个列表的总记录可能已经达到 2c。
      }
      mru_.push_front(frame_id);
      auto status =std::make_shared<FrameStatus>(page_id,frame_id,false,ArcStatus::MRU);
      status->alive_pos_=mru_.begin();
      alive_map_.emplace(frame_id,status);
      return ;
    }
    auto status = ghost_it->second;

    if(status->arc_status_ == ArcStatus::MRU_GHOST){
      //命中 MRU ghost：增大 MRU 的目标大小。
      size_t delta = std::max(size_t{1},mfu_ghost_.size()/mru_ghost_.size());
      mru_target_size_=std::min(replacer_size_,mru_target_size_+delta);
      mru_ghost_.erase(status->ghost_pos_);
    }else{
      // 命中 MFU ghost：减小 MRU 的目标大小。
      size_t delta =std::max(size_t{1},mru_ghost_.size()/mfu_ghost_.size());
      if(delta>=mru_target_size_)
      mru_target_size_=0;
      else
      mru_target_size_-=delta;
      mfu_ghost_.erase(status->ghost_pos_);
    }
    ghost_map_.erase(ghost_it);

    status->frame_id_=frame_id;
    status->evictable_=false;
    mfu_.push_front(frame_id);
    status->arc_status_=ArcStatus::MFU;
    status->alive_pos_= mfu_.begin();

    alive_map_.emplace(frame_id,status);
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  BUSTUB_ENSURE(
    frame_id >= 0 && static_cast<size_t>(frame_id) <= replacer_size_,
    "invalid frame id");//frame号不在范围，报错
  std::scoped_lock lock(latch_);
  auto it = alive_map_.find(frame_id);
  if(it == alive_map_.end()){
    return ;
  }
  auto status = it->second;
  if(set_evictable != status->evictable_){
    status->evictable_=set_evictable;
    if(set_evictable == false){
      curr_size_--;
    }
    else{
      curr_size_++;
    }
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * decided by the ARC algorithm.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void ArcReplacer::Remove(frame_id_t frame_id) {//删除arc驻留记录
  std::scoped_lock lock(latch_);
  auto it = alive_map_.find(frame_id); 
  if(it!=alive_map_.end()){
    auto status = it->second;
    BUSTUB_ENSURE(status->evictable_,"can't remove");
    if(status->arc_status_==ArcStatus::MFU)
    mfu_.erase(status->alive_pos_);
    else
    mru_.erase(status->alive_pos_);
    alive_map_.erase(it);
    curr_size_--;
  }
  return ;
}

auto ArcReplacer::Size() -> size_t {
  std::scoped_lock lock(latch_);
  return curr_size_;
}

}  // namespace bustub
