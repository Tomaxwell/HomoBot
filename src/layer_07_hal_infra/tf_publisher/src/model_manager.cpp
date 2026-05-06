#include "tf_publisher/model_manager.hpp"

#include "urdf/model.h"

namespace tf_publisher
{

ModelManager::ModelManager() = default;
ModelManager::~ModelManager() = default;

bool ModelManager::load_from_string(const std::string & urdf_string)
{
  model_ = std::make_unique<urdf::Model>();
  if (!model_->initString(urdf_string)) {
    model_.reset();
    return false;
  }
  return true;
}

bool ModelManager::is_loaded() const
{
  return model_ != nullptr;
}

const urdf::Model * ModelManager::get_model() const
{
  return model_.get();
}

void ModelManager::unload()
{
  model_.reset();
}

}  // namespace tf_publisher
