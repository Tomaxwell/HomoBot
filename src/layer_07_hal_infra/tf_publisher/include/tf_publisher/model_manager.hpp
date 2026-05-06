#ifndef TF_PUBLISHER__MODEL_MANAGER_HPP_
#define TF_PUBLISHER__MODEL_MANAGER_HPP_

#include <string>
#include <memory>

namespace urdf
{
class Model;
}

namespace tf_publisher
{

/**
 * @brief URDF model manager
 *
 * Loads and caches the URDF robot description.
 */
class ModelManager
{
public:
  ModelManager();
  ~ModelManager();

  /**
   * @brief Load robot model from URDF string
   * @return true on success
   */
  bool load_from_string(const std::string & urdf_string);

  bool is_loaded() const;
  const urdf::Model * get_model() const;

  void unload();

private:
  std::unique_ptr<urdf::Model> model_;
};

}  // namespace tf_publisher

#endif  // TF_PUBLISHER__MODEL_MANAGER_HPP_
