#pragma once
#include "node.h"
#include "svulkan2/resource/model.h"
#include <unordered_map>

namespace svulkan2 {

struct CustomData {
  DataType dtype;
  union {
    float floatValue;
    glm::vec2 float2Value;
    glm::vec3 float3Value;
    glm::vec4 float4Value;
    glm::mat4 float44Value;

    int intValue;
    glm::ivec2 int2Value;
    glm::ivec3 int3Value;
    glm::ivec4 int4Value;
  };
};

namespace scene {
class Object : public Node {
  std::shared_ptr<resource::SVModel> mModel;
  glm::uvec4 mSegmentation{0};
  std::unordered_map<std::string, CustomData> mCustomData;
  int mShadingMode{};
  float mTransparency{};

public:
  inline Type getType() const override { return Type::eObject; }

  Object(std::shared_ptr<resource::SVModel> model,
         std::string const &name = "");

  void uploadToDevice(core::Buffer &objectBuffer,
                      StructDataLayout const &objectLayout);

  void setSegmentation(glm::uvec4 const &segmentation);
  inline glm::uvec4 getSegmentation() const { return mSegmentation; }
  inline std::shared_ptr<resource::SVModel> getModel() const { return mModel; }

  void setCustomDataFloat(std::string const &name, float x);
  void setCustomDataFloat2(std::string const &name, glm::vec2 x);
  void setCustomDataFloat3(std::string const &name, glm::vec3 x);
  void setCustomDataFloat4(std::string const &name, glm::vec4 x);
  void setCustomDataFloat44(std::string const &name, glm::mat4 x);
  void setCustomDataInt(std::string const &name, int x);
  void setCustomDataInt2(std::string const &name, glm::ivec2 x);
  void setCustomDataInt3(std::string const &name, glm::ivec3 x);
  void setCustomDataInt4(std::string const &name, glm::ivec4 x);

  /** used to choose gbuffer pipelines */
  inline void setShadingMode(int mode) { mShadingMode = mode; }
  inline int getShadingMode() const { return mShadingMode; }

  inline void setTransparency(float transparency) {
    mTransparency = transparency;
  }
  inline float getTransparency() const { return mTransparency; }
};

} // namespace scene
} // namespace svulkan2