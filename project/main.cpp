//#include<Windows.h>

//自作h
#include"DebugCamera.h"
#include"DirectXCom.h"
#include"KeyInput.h"
#include"Log.h"
#include"Matrix4x4.h"
#include"Sprite.h"
#include"SpriteCom.h"
#include"ResourceLeakCheak.h"
#include"Sound.h"
#include"Vector.h"
#include"WindowsAPI.h"

#include<chrono> //時間を扱うライブラリ
#include<filesystem> //ファイルやディレクトリに関する操作を行うライブラリ
#include<format> //文字列のフォーマットを行うライブラリ
#include<fstream> //ファイルにかいたり読んだりするライブラリ
#include<string> //文字列を扱うライブラリ
#include<strsafe.h>

#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>



//Comptr
#include<wrl.h>

//Debug用
#include<dbghelp.h>
#pragma comment(lib,"Dbghelp.lib")

//ファイル関係 / サウンド関係
#include<sstream>
//#include <xaudio2.h>
//#pragma comment(lib, "xaudio2.lib")


//ReportLiveObjects
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")

//DXCの初期化
#include<dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

//Textureの転送
#include"externals/DirectXTex/d3dx12.h"
#include<vector>

#include <DirectXMath.h>
#include<cmath>
#include "externals/DirectXTex/DirectXTex.h"


struct Transform
{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};



struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3]; // パディングを追加して16バイト境界に揃える
	Matrix4x4 uvTransform; // UV変換行列
};

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData
{
	std::string textureFilePath; // テクスチャファイルのパス
};

//objファイル関係
struct ModelData
{
	std::vector<Sprite::VertexData> vertices; // 頂点データ
	MaterialData material; // マテリアルデータ
};





enum BlendMode
{
	//!< ブレンドなし
	kBlendMode_None,

	//!< αブレンド
	kBlendMode_Normal,

	//!< 加算ブレンド
	kBlendMode_Add,

	//!< 減算ブレンド
	kBlendMode_Sub,

	//!< 乗算ブレンド
	kBlendMode_Mul,

	//!< スクリーンブレンド
	kBlendMode_Screen,

	//利用禁止
	kCountOfBlendMode,
};








static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	//時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetSystemTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	//processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();

	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{};
	minidumpInformation.ThreadId = threadId; //クラッシュしたスレッドのID
	minidumpInformation.ExceptionPointers = exception; //例外情報
	minidumpInformation.ClientPointers = TRUE; //クライアントポインタを使用する

	//Dumpの出力を行う。MiniDumpNormalは最小限の情報を出力する
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

	//ほかに関連付けられているSEH例外ハンドラがあったら実行。通常時はプロセスを終了
	return EXCEPTION_EXECUTE_HANDLER;
}









//TextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);//Textureの幅
	resourceDesc.Height = UINT(metadata.height);//Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);//mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);//奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);//Textureの次元数。普段使っているのは2次元

	//2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//細かい設定を行う
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//WriteBackポリシーでCPUアクセス可能
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//プロセッサの近くに配列

	//3. Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,//初回のResourceState。Textureは基本読むだけ
		nullptr,//Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}



MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
	//中で必要になる変数の宣言
	MaterialData materlialData;//構築するデータ
	std::string line;//ファイルから読み込んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//ファイルが開けなかったら停止

	//MaterialDataを構築
	while (std::getline(file, line))
	{
		std::string identifile;
		std::istringstream s(line);
		s >> identifile; //先頭の識別子を取得

		//identifileに応じた処理
		if (identifile == "map_Kd")
		{
			std::string textureFilename;
			s >> textureFilename; //テクスチャファイル名を取得
			//連結してファイルパスにする
			materlialData.textureFilePath = directoryPath + "/" + textureFilename;
		}

	}



	return materlialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
	//中で必要になる変数の宣言
	ModelData modelData;//構築するデータ
	std::vector<Vector4>positions;//位置
	std::vector<Vector3>normals;//法線
	std::vector<Vector2>texcoords;//テクスチャ座標
	std::string line;//ファイルから読み込んだ1行を格納するもの

	//ファイルを開く
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//ファイルが開けなかったら停止

	//実際にファイルを読み込む。その後modelDataを構築する
	while (std::getline(file, line))
	{
		std::string identifile;
		std::istringstream s(line);
		s >> identifile; //先頭の識別子を取得

		//identifileに応じた処理
		if (identifile == "v")
		{
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.x *= -1.0f; //X軸を反転する
			position.w = 1.0f;
			positions.push_back(position);//位置を格納
		}
		else if (identifile == "vt")
		{
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y; //Y軸を反転する
			texcoords.push_back(texcoord);//テクスチャ座標を格納
		}
		else if (identifile == "vn")
		{
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.x *= -1.0f; //X軸を反転する
			normals.push_back(normal);//法線を格納
		}
		else if (identifile == "f")
		{
			//面は三角形限定。その他は未対応
			Sprite::VertexData triangle[3];

			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex)
			{
				std::string vertexDefinition;
				s >> vertexDefinition; //頂点の定義を取得

				//頂点の要素へのIndexは、位置、UV、法線の順で入っているため、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element)
				{
					std::string index;
					std::getline(v, index, '/'); //スラッシュで区切って要素を取得
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから実際の用をの値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1]; //OBJファイルは1始まりなので-1する
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				//VertexData vertex = { position, texcoord, normal };
				//modelData.vertices.push_back(vertex); //頂点を格納
				triangle[faceVertex] = { position, texcoord, normal };
			}
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
		else if (identifile == "mtllib")
		{
			//materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename; //マテリアルファイル名を取得
			//基本的にobjファイルを同じ階層にmtlファイルがあるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}

	}

	return modelData;
}



//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{



	ResourceLeakCheak leakChecker; //リソースリークチェック用のオブジェクト

	CoInitializeEx(0, COINIT_MULTITHREADED);

	//誰も補足しなかった場合に(Unhandled)、補足する関数を登録
	SetUnhandledExceptionFilter(ExportDump);


	//ログファイル関係
	//ログのディレクトリを用意
	std::filesystem::create_directories("logs");

	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	//ログファイルの名前にコンマ何秒はいらないため、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSecound = std::chrono::time_point_cast<std::chrono::seconds>(now);

	//日本時間(PCの設定時間に変換)
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSecound };

	//formatを使って年月日_時分秒の形式にする
	std::string datString = std::format("%Y%m%d_%H%M%S", localTime);

	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + datString + ".log";

	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	WindowAPI* windowAPI = nullptr; //ウィンドウ関連のAPIをまとめたオブジェクト
	windowAPI = new WindowAPI();
	windowAPI->Initialize();

	DirectXCom* dxCommon = nullptr;
	dxCommon = new DirectXCom(windowAPI, logStream);
	
	dxCommon->DebugLayer();

	//ウィンドウを表示する
	windowAPI->Show();

	dxCommon->Initialize();

	SpriteCom* spriteCom = nullptr;
	spriteCom = new SpriteCom(logStream,dxCommon);
	spriteCom->Initialize();

	Sprite* sprite = nullptr;
	sprite = new Sprite();
	sprite->Initialize(spriteCom);


	spriteCom->CreateGraphicsPipeline();


	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込み
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	//比較関数はLessEqua。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//DepthStencilの設定
	spriteCom->GetGraphicPipelineStateDesc().DepthStencilState = depthStencilDesc;
	spriteCom->GetGraphicPipelineStateDesc().DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//実際に生成
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicPipelineState = nullptr;
	dxCommon->SetHr(dxCommon->GetDevice()->CreateGraphicsPipelineState(&spriteCom->GetGraphicPipelineStateDesc(),
		IID_PPV_ARGS(&graphicPipelineState)));

	assert(spriteCom->GetVertexShaderBlob() && "頂点シェーダーの読み込み失敗！");
	assert(spriteCom->GetPixelShaderBlob() && "ピクセルシェーダーの読み込み失敗！");

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(dxCommon->GetHr()));

	// 球体
	const uint32_t kSubdivision = 16; // 16分割

	// 経度分割1つ分の角度
	const float kLonEvery = DirectX::XM_2PI / float(kSubdivision);
	// 緯度分割1つ分の角度
	const float kLatEvery = DirectX::XM_PI / float(kSubdivision);

	// 頂点数・インデックス数
	// 緯度方向と経度方向の両端に重複する頂点があるため、+1が必要
	const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	const uint32_t kIndexCount = kSubdivision * kSubdivision * 6; // 各四角形に三角形2つ、各三角形に頂点3つで 2*3=6

	// 頂点配列を確保
	Sprite::VertexData* vertexData = new Sprite::VertexData[kVertexCount];

	// --- 頂点データを埋める ---
	for (uint32_t lat = 0; lat <= kSubdivision; ++lat)
	{
		// 緯度 (theta): -π/2 (下端) から π/2 (上端) まで
		float theta = -DirectX::XM_PIDIV2 + DirectX::XM_PI * (float(lat) / kSubdivision);
		for (uint32_t lon = 0; lon <= kSubdivision; ++lon)
		{
			// 経度 (phi): 0 (東端) から 2π (一周) まで
			float phi = DirectX::XM_2PI * (float(lon) / kSubdivision);
			uint32_t idx = lat * (kSubdivision + 1) + lon; // 1次元配列内のインデックス

			// 球面座標からデカルト座標への変換
			vertexData[idx].position.x = cos(theta) * cos(phi);
			vertexData[idx].position.y = sin(theta);
			vertexData[idx].position.z = cos(theta) * sin(phi);
			vertexData[idx].position.w = 1.0f; // 同次座標

			// テクスチャ座標 (UV)
			// U: 経度に比例 (0.0 から 1.0)
			vertexData[idx].texcoord.x = float(lon) / kSubdivision;
			// V: 緯度に比例 (1.0 から 0.0、上向きが正になるように反転)
			vertexData[idx].texcoord.y = 1.0f - float(lat) / kSubdivision;

			// 法線ベクトル (原点から頂点へのベクトルがそのまま法線となる)
			vertexData[idx].normal = {
				vertexData[idx].position.x,
				vertexData[idx].position.y,
				vertexData[idx].position.z
			};
		}
	}


	// --- 頂点バッファを作成・アップロード ---
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Sprite::VertexData) * kVertexCount);
	Sprite::VertexData* mappedVertex = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertex));
	memcpy(mappedVertex, vertexData, sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);

	uint32_t* indexData = new uint32_t[kIndexCount];
	uint32_t idx = 0; // ここを元のままの変数名に戻しました

	for (uint32_t lat = 0; lat < kSubdivision; ++lat)
	{
		for (uint32_t lon = 0; lon < kSubdivision; ++lon)
		{

			uint32_t v0 = lat * (kSubdivision + 1) + lon;             // 左上 (A)
			uint32_t v1 = v0 + 1;                                      // 右上 (C)
			uint32_t v2 = v0 + (kSubdivision + 1);                     // 左下 (B)
			uint32_t v3 = v2 + 1;                                      // 右下 (D)

			// 四角形を2つの三角形で表現する
			// 1つ目の三角形: v0, v2, v1 (A, B, C)
			// DirectXでは通常、右手座標系で反時計回り（CCW）が表
			indexData[idx++] = v0; // A
			indexData[idx++] = v2; // B
			indexData[idx++] = v1; // C

			// 2つ目の三角形: v2, v3, v1 (B, D, C)
			indexData[idx++] = v2; // B
			indexData[idx++] = v3; // D
			indexData[idx++] = v1; // C
		}
	}

	// --- インデックスバッファを作成・アップロード ---
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSphere = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(uint32_t) * kIndexCount);
	uint32_t* mappedIndex = nullptr;
	indexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	memcpy(mappedIndex, indexData, sizeof(uint32_t) * kIndexCount);
	indexResourceSphere->Unmap(0, nullptr);

	// --- バッファビュー設定 ---
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();
	vertexBufferViewSphere.SizeInBytes = sizeof(Sprite::VertexData) * kVertexCount;
	vertexBufferViewSphere.StrideInBytes = sizeof(Sprite::VertexData);

	D3D12_INDEX_BUFFER_VIEW indexBufferViewSphere{};
	indexBufferViewSphere.BufferLocation = indexResourceSphere->GetGPUVirtualAddress();
	indexBufferViewSphere.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferViewSphere.Format = DXGI_FORMAT_R32_UINT;

	Sprite::VertexData* mapped = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
	memcpy(mapped, vertexData, sizeof(Sprite::VertexData) * kVertexCount);
	vertexResourceSphere->Unmap(0, nullptr);


	//モデル読み込み
	ModelData modelData = LoadObjFile("Resources", "plane.obj");
	//頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceModel = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Sprite::VertexData) * modelData.vertices.size());
	//頂点バッファービューを作成末う
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResourceModel->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(Sprite::VertexData) * modelData.vertices.size()); //使用するリソースのサイズは頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(Sprite::VertexData); //1頂点当たりのサイズ
	//頂点リソースにデータを書き込む
	Sprite::VertexData* vertexDataModel = nullptr;
	vertexResourceModel->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataModel));
	std::memcpy(vertexDataModel, modelData.vertices.data(), sizeof(Sprite::VertexData) * modelData.vertices.size());//頂点データをリソースにコピー

	//Sprite用の頂点Resource
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Sprite::VertexData) * 6);
	//頂点バッファビューを生成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof(Sprite::VertexData) * 6;
	//1頂点当たりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(Sprite::VertexData);


	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(uint32_t) * 6);
	//頂点バッファービューを生成する
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;

	//インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; // 左下
	indexDataSprite[1] = 1; // 左上
	indexDataSprite[2] = 2; // 右下
	indexDataSprite[3] = 2; // 右下
	indexDataSprite[4] = 1; // 左上
	indexDataSprite[5] = 3; // 右上

	indexResourceSprite->Unmap(0, nullptr);

	//Sprite用
	Sprite::VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	//一枚目の三角形
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f }; // 左下
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };   // 左上
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f }; // 右下
	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f };   // 右上

	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };




	//マテリアル用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Material));
	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	//書き込む為のアドレス取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// データを設定（赤色 RGBA: 1,0,0,1）
	Vector4 temp{};
	temp.x = 1.0f;
	temp.y = 1.0f;
	temp.z = 1.0f;
	temp.w = 1.0f;
	materialData->color = temp;
	materialData->enableLighting = false;
	materialResource->Unmap(0, nullptr);

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLight = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(DirectionalLight));

	// MapしてGPUリソースのCPU側の書き込み可能ポインタを取得する
	DirectionalLight* directionalLightData = nullptr;
	directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// directionalLightDataに値を書き込む
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;



	// 書き込み完了後はUnmapを呼ぶ
	directionalLight->Unmap(0, nullptr);

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(Material));
	Material* materialDataSprite = nullptr;
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	materialDataSprite->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（テクスチャ色をそのまま出す用）
	materialDataSprite->enableLighting = false;
	materialResourceSprite->Unmap(0, nullptr);


	//WVP用のリソースを作る。　Matrix4x4 1つのサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込む為のアドレス取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込む
	wvpData->World = MakeIdentity4x4();
	wvpData->WVP = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSphere = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));

	// データを書き込むためのポインタを取得
	TransformationMatrix* transformationMatrixDataSphere = nullptr;
	transformationMatrixResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSphere));
	transformationMatrixDataSphere->WVP = MakeIdentity4x4();
	transformationMatrixDataSphere->World = MakeIdentity4x4();
	// 書き込みが完了したので、マップを解除
	transformationMatrixResourceSphere->Unmap(0, nullptr);

	//Sprite用のTransformationMatrix用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = dxCommon->CreateBufferResource(dxCommon->GetDevice().Get(), sizeof(TransformationMatrix));
	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	//書き込むためのアドレス取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	//単位行列を書き込んでおく
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();
	transformationMatrixResourceSprite->Unmap(0, nullptr);

	//Transform変数を作る
	Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	//Sprite用
	Transform transformSprite{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	//Sphere用
	Transform transformSphere{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	Transform cameraTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };

	//uvTrandform用の変数
	Transform uvTransformSprite = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	//uvTransform行列の初期化
	materialData->uvTransform = MakeIdentity4x4();
	materialDataSprite->uvTransform = MakeIdentity4x4();

	float fovY = 0.45f;  // 資料通り
	float aspectRatio = static_cast<float>(windowAPI->GetClientWidth()) / static_cast<float>(windowAPI->GetClientHeight());
	float nearZ = 0.1f;
	float farZ = 100.0f;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = dxCommon->LoadTexture("./Resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(dxCommon->GetDevice(), metadata);
	
	//2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = dxCommon->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(dxCommon->GetDevice(), metadata2);

	

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = dxCommon->UploadTextureData(textureResource, mipImages, dxCommon->GetDevice().Get(), dxCommon->GetCommandList());
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2 = dxCommon->UploadTextureData(textureResource2, mipImages2, dxCommon->GetDevice().Get(), dxCommon->GetCommandList());

	//metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//二つ目。metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	
	//SRVを生成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = dxCommon->GetSrvDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon->GetSrvDescriptorHeap()->GetGPUDescriptorHandleForHeapStart();

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = dxCommon->GetCPUDescroptirHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = dxCommon->GetGPUDescriptorHandle(dxCommon->GetSrvDescriptorHeap(), dxCommon->GetDescriptorSizeSRV(), 2);

	//先頭はImGuiに使用しているためその次を使う
	textureSrvHandleCPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += dxCommon->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//SRVの生成
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
	//2つ目
	dxCommon->GetDevice()->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	//SRVの切り替え
	bool useMonsterBall = true;
	//Sphereの描画切り替え
	bool drawSphere = true;
	bool drawSprite = true;

	
	//音声読み込み
	Sound* sound_ = nullptr;
	sound_ = new Sound();
	sound_->Initialize();
	//音声再生
	SoundData soundData = sound_->SoundLoadWave("Resources/Alarm01.wav");
	sound_->SoundPlayWave(sound_->GetXAudio2(), soundData);

	KeyInput inputManager;
	inputManager.Initialize(windowAPI);

	DebugCamera debugCamera_;
	debugCamera_.Initialize(windowAPI);


	//ウィンドウのxボタンが押されるまでループ
	while (dxCommon->GetMsg().message != WM_QUIT)
	{
		////Windowに目セージが来ていたら最優先で処理される
		if (PeekMessage(&dxCommon->GetMsg(), NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&dxCommon->GetMsg()); //メッセージを変換
			DispatchMessage(&dxCommon->GetMsg()); //メッセージをウィンドウプロシージャに送る
		}

		else
		{

			//if (windowAPI->ProcessMassage())
			//{
			//	//ゲームループ抜ける
			//	break;
			//}


			//Imguiにここからフレームが始まる趣旨をつたえる
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			debugCamera_.Update();

			//ゲームの処理
			/*transformSphere.rotate.y += 0.01f;*/
			Matrix4x4 worldMatrix = MakeAffineMatrix(transformSphere.scale, transformSphere.rotate, transformSphere.translate);
			Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = Inverse(cameraMatrix);
			Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearZ, farZ);
			//WVPMatrixを作る
			Matrix4x4 worldViewProjectMatrix = Multiply(worldMatrix, Multiply(debugCamera_.GetViewMatrix(), projectionMatrix));
			transformationMatrixDataSphere->WVP = worldViewProjectMatrix;
			transformationMatrixDataSphere->World = worldMatrix;

			//Sprite用のworldViewProjectMatrix
			Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(windowAPI->GetClientWidth()), float(windowAPI->GetClientHeight()), 0.0f, 100.0f);
			Matrix4x4 worldViewProjectionmatrixSprite = Multiply(worldMatrixSprite, Multiply(debugCamera_.GetViewMatrix(), projectionMatrixSprite));
			transformationMatrixDataSprite->WVP = worldViewProjectionmatrixSprite;
			transformationMatrixDataSprite->World = worldMatrixSprite;

			//UVTransform用
			Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

			directionalLight->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));


			//開発用UIの処理、実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換え

#ifdef _DEBUG



			ImGui::ShowDemoWindow();

			ImGui::Begin("Windows");


			ImGui::ColorEdit4("Material Color", &materialData->color.x);
			ImGui::DragFloat("Light Intensity", &directionalLightData->intensity, 0.01f, 0.0f, 10.0f);


			ImGui::Checkbox("useMonsterBall", &useMonsterBall);
			ImGui::Checkbox("LightSprite Flag", (bool*)&materialData->enableLighting);
			ImGui::Checkbox("LightSphere Flag", (bool*)&materialDataSprite->enableLighting);

			ImGui::Checkbox("DrawSphere", &drawSphere);
			ImGui::Checkbox("DrawSprite", &drawSprite);
			ImGui::DragFloat3("LightDirection", &directionalLightData->direction.x, 0.01f);

			ImGui::DragFloat3("Sphere Rotate", &transformSphere.rotate.x, 0.01f, -10.0f, 10.0f);

			ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
			ImGui::DragFloat("UVRotate", &uvTransformSprite.rotate.z, 0.01f);


			ImGui::End();

#endif // DEBUG

			//ImGui内部コマンドを生成する
			ImGui::Render();

			inputManager.Update();

			dxCommon->PreDraw();

			spriteCom->SetupDraw();

		
		//RootSignatureを設定。PSOに設定しているけれど別途設定が必要
			dxCommon->GetCommandList()->SetGraphicsRootSignature(spriteCom->GetRootSignature().Get());
			dxCommon->GetCommandList()->SetPipelineState(graphicPipelineState.Get()); //パイプラインステートを設定
			//Sphereの描画

			dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
			dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewSphere);
			dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSphere->GetGPUVirtualAddress());
			dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());
			if (drawSphere)
			{
				/*commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);*/
				dxCommon->GetCommandList()->DrawInstanced(UINT(modelData.vertices.size()), 1, 0, 0);
			}

			if (drawSprite)
			{
				dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
				dxCommon->GetCommandList()->IASetIndexBuffer(&indexBufferViewSprite);
				dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());
				dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall ? textureSrvHandleGPU2 : textureSrvHandleGPU);
				dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLight->GetGPUVirtualAddress());

				dxCommon->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
			}


			//実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList().Get());


			dxCommon->PostDraw();
		}

	}

	//ImGui終了処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();


	Logger::Log(logStream, "Application terminating.");

	std::wstring wstringValue = L"Hello, DirectX!";
	Logger::Log(logStream, StringUtil::ConvertString(std::format(L"WSTRING{}\n", wstringValue)));


	//出力ウィンドウへの文字出力
	OutputDebugStringA("Hello, DirextX!\n");

	CloseHandle(dxCommon->GetFenceEvent());

	sound_->GetXAudio2().Reset();
	sound_->SoundUnload(&soundData);

	delete sound_;

	delete[] vertexData;
	delete[] indexData;

	windowAPI->Finalize();

	delete sprite;
	delete spriteCom;

	delete dxCommon;

	delete windowAPI;

	return 0;
}

