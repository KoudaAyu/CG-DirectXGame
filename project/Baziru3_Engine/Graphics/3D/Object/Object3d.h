#pragma once

#include "AnimationData.h"
#include "Animator.h"
#include "Camera.h"
#include "MaterialManager.h"
#include "NodeAnimation.h"
#include "Skeleton.h"
#include "SkinCluster.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "Transform.h"
#include <string>
#include <string_view>
#include <vector>

class Object3dCom;
class SkinningObject3dCom;
class RenderContext;

class Object3d {
public:
  static const std::vector<Object3d *> &GetInstances() { return instances_; }

private:
  static std::vector<Object3d *> instances_;

public:
  // GPU用の定数バッファ(CB)レイアウトには MaterialManager.h のグローバルな
  // `Material` を使用

  struct MaterialData {
    std::string textureFilePath;
    uint32_t textureIndex = 0;
  };

  struct ModelData {
    std::vector<Sprite::VertexData> vertices; // 頂点データ
    std::vector<uint32_t> indices;            // インデックスデータ
    MaterialData material;                    // マテリアルデータ
    NodeAnimation rootNode;                   // 階層構造のルートノード
    float boundingRadius = 2.0f;              // バウンディングスフィア半径
  };

  struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
  };

  struct TransformationMatrixData {
    Matrix4x4 WVP;
    Matrix4x4 World;
  };

  struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
  };

  struct PointLight {
    Vector4 color;     // ライトカラー (RGB) + 未使用 (A)
    Vector3 position;  // 3D座標
    float intensity;   // 輝度
    float radius;      // 影響半径
    float decay;       // 減衰率 (2.0f: 逆二乗則準拠)
    float padding[2];  // 16バイト境界アライメント
  };

  struct LightGroup {
    DirectionalLight directionalLight;
    PointLight pointLight;
  };

  Object3d();

  // 簡易初期化：SceneManager から Object3dCom を自動取得してモデルを読み込む（最も推奨される初期化方法）
  // 引数 directoryPath: モデルファイルのあるディレクトリ（例: "Resources"）
  // 引数 filename: モデルファイル名（.obj, .gltf 等、例: "plane.obj"）
  // 戻り値: 成功時 true、失敗時 false
  bool Initialize(std::string_view directoryPath, std::string_view filename);

  void Initialize(Object3dCom *object3dCom, const ModelData &modelData,
                  TextureManager *textureManager = nullptr);
  void InitializeShared(Object3dCom *object3dCom, Object3d *masterObject);

  // アニメーション・スケルトン・スキンクラスターをまとめてセットアップする
  // アプリ側で読み込んだ Animation と Skeleton、Model::ModelData を渡す
  void SetupAnimation(const Animation *animation, const Skeleton &skeleton,
                      const Model::ModelData &modelData);

  // トランスフォーム行列・カリング状態を更新する（毎フレーム Draw() 前に呼ぶこと）
  void Update();

  // 3Dオブジェクトを描画する（最も推奨される描画方法。内部でパイプライン・テクスチャ・カメラ・ライトを自動解決）
  // 引数 object3dCom: 指定がなければ内部保持または SceneManager から自動取得
  // 引数 skinningObject3dCom: スキニングアニメーション時の描画コンポーネント（不要なら省略可）
  void Draw(Object3dCom *object3dCom = nullptr, SkinningObject3dCom *skinningObject3dCom = nullptr);
  void Draw(const RenderContext &ctx);

  /// <summary>
  /// .mtlファイルの読み込み
  /// </summary>
  /// <param name="directoryPath"></param>
  /// <param name="filename"></param>
  /// <returns></returns>
  static MaterialData LoadMaterialTemplateFile(const std::string &direcrotyPath,
                                               const std::string &filename);

  /// <summary>
  /// .objファイルの読み込み
  /// </summary>
  /// <param name="directoryPath">ファイルパス</param>
  /// <param name="filename">.objパス</param>
  /// <returns></returns>
  static ModelData LoadObjFile(const std::string &directoryPath,
                               const std::string &filename);

  /// <summary>
  /// Assimp対応モデルファイルの読み込み(.gltf など)
  /// </summary>
  /// <param name="directoryPath">ファイルパス</param>
  /// <param name="filename">モデルファイル名</param>
  /// <returns></returns>
  static ModelData LoadModelFile(const std::string &directoryPath,
                                 const std::string &filename);

  void VertexResource();
  void MaterialResource();
  void TransformationMatrixResource();
  void DirectionalLightResource();

  void Finalize() {}
  ~Object3d();

public:
  void SetCamera(Camera *camera) { camera_ = camera; }
  Camera *GetCamera() const { return camera_; }
  void SetObject3dCom(Object3dCom *object3dCom) { object3dCom_ = object3dCom; }

  D3D12_GPU_VIRTUAL_ADDRESS GetTransformationMatrixGPUAddress() const {
    return transformationMatrixGpuAddress_;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetMaterialGPUAddress() const {
    return materialGpuAddress_;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetDirectionalLightGPUAddress() const {
    return lightGpuAddress_;
  }
  D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUAddress() const {
    return lightGpuAddress_;
  }
  LightGroup &GetLightData() { return lightData_; }
  const LightGroup &GetLightData() const { return lightData_; }
  const Microsoft::WRL::ComPtr<ID3D12Resource> &GetVertexResource() const {
    return vertexResource;
  }
  void PrepareConstantBuffers(DirectXCom *dx);

  const D3D12_VERTEX_BUFFER_VIEW &GetVertexBufferView() const {
    return vertexBufferView_;
  }
  const D3D12_INDEX_BUFFER_VIEW &GetIndexBufferView() const {
    return indexBufferView_;
  }
  bool HasIndexBuffer() const {
    return indexResource != nullptr && !modelData_.indices.empty();
  }

  void SetRotate(const Vector3 &r) { transform.SetRotate(r); }
  void SetTranslate(const Vector3 &t) { transform.SetTranslate(t); }
  void SetScale(const Vector3 &s) { transform.SetScale(s); }
  void SetTransform(const Sprite::Transform &t) {
    transform.SetTranslate(t.translate);
    transform.SetRotate(t.rotate);
    transform.SetScale(t.scale);
  }
  const Vector3 &GetRotate() const { return transform.GetRotate(); }
  const Vector3 &GetTranslate() const { return transform.GetTranslate(); }
  const Vector3 &GetScale() const { return transform.GetScale(); }
  const ModelData &GetModelData() const { return modelData_; }
  const Matrix4x4 &GetWorldMatrix() const {
    return transformationMatrixData_.World;
  }

  void MarkDrawn() { isDrawnThisFrame_ = true; }
  bool WasDrawnLastFrame() const { return wasDrawnLastFrame_; }
  bool IsCulled() const { return isCulled_; }
  void ResetFrameDrawFlags() {
    wasDrawnLastFrame_ = isDrawnThisFrame_;
    isDrawnThisFrame_ = false;
  }

  void SetEnableLighting(bool enable);
  void SetColor(const Vector4 &color);
  void SetReflectionFactor(float factor);
  void SetFresnelF0(float f0);

  void SetDeltaTime(float dt) { deltaTime_ = dt; }
  bool HasAnimation() const { return animator_.HasAnimation(); }
  Skeleton &GetSkeleton() { return skeleton_; }
  const Skeleton &GetSkeleton() const { return skeleton_; }
  const SkinCluster &GetSkinCluster() const { return skinCluster_; }
  bool IsShared() const { return isShared_; }

public:
  void DrawInternal(const RenderContext &ctx);

private:
  Transform transform;

  Transform cameraTransform;

private:
  Camera *camera_ = nullptr;
  Object3dCom *object3dCom_ = nullptr;
  Transform transform_;

  ModelData modelData_; // モデルデータを保持

  // バッファリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
  // バッファリソース内のデータを指すポインタ
  VertexData *vertexData_ = nullptr;
  // バッファリソースの使い道を補足するバッファビュー
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
  // インデックスバッファ用のリソース
  Microsoft::WRL::ComPtr<ID3D12Resource> indexResource = nullptr;
  D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

  // 定数データの実体
  Material materialData_{};
  TransformationMatrix transformationMatrixData_{};
  LightGroup lightData_{};

  // 毎フレーム割り当てられるGPU仮想アドレスのキャッシュ
  D3D12_GPU_VIRTUAL_ADDRESS materialGpuAddress_ = 0;
  D3D12_GPU_VIRTUAL_ADDRESS transformationMatrixGpuAddress_ = 0;
  D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress_ = 0;

  // アニメーション / スケルトン / スキンクラスター
  Animator animator_;
  Skeleton skeleton_;
  SkinCluster skinCluster_;
  SkinClusterLender skinClusterLender_;
  bool skinClusterInitialized_ = false;
  float deltaTime_ = 1.0f / 60.0f;
  TextureManager *textureManager_ = nullptr;
  bool isDrawnThisFrame_ = false;
  bool wasDrawnLastFrame_ = false;
  bool isShared_ = false;
  bool isCulled_ = false;
  Object3d *masterObject_ = nullptr;
};