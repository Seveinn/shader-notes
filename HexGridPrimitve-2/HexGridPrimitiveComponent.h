// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"  // 添加这个头文件以获取FProcMeshTangent
#include "HexGridPrimitiveComponent.generated.h"

// 前向声明
class FHexGridPrimitiveSceneProxy;

// 渲染模式枚举
UENUM(BlueprintType)
enum class EHexGridRenderMode : uint8
{
	SingleHexagon		UMETA(DisplayName = "单个六棱柱"),
	HexGrid			UMETA(DisplayName = "网格六棱柱")
};

// 地块类型枚举
UENUM(BlueprintType)
enum class EHexTileType : uint8
{
	Grass		UMETA(DisplayName = "草地"),
	Water		UMETA(DisplayName = "水域"),
	Mountain	UMETA(DisplayName = "山地"),
	Desert		UMETA(DisplayName = "沙漠"),
	Forest		UMETA(DisplayName = "森林"),
	Swamp		UMETA(DisplayName = "沼泽")
};

// 地块类型配置结构体
USTRUCT(BlueprintType)
struct FHexTileTypeConfig
{
	GENERATED_BODY()

	// 地块类型
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块配置")
	EHexTileType TileType = EHexTileType::Grass;

	// 颜色参数（对应材质中的color参数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块配置", meta = (DisplayName = "基础颜色"))
	FLinearColor BaseColor = FLinearColor::Green;

	// 颜色乘数（对应材质中的ColorMultiply参数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块配置", meta = (DisplayName = "颜色乘数"))
	FLinearColor ColorMultiply = FLinearColor::White;

	// 金属度（对应材质中的metallic参数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块配置", meta = (DisplayName = "金属度", ClampMin = "0.0", ClampMax = "1.0"))
	float Metallic = 0.0f;

	// 高光（对应材质中的specular参数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块配置", meta = (DisplayName = "高光", ClampMin = "0.0", ClampMax = "1.0"))
	float Specular = 0.0f;

	// 粗糙度（对应材质中的roughness参数）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块配置", meta = (DisplayName = "粗糙度", ClampMin = "0.0", ClampMax = "1.0"))
	float Roughness = 0.0f;

	FHexTileTypeConfig()
	{
		TileType = EHexTileType::Grass;
		BaseColor = FLinearColor::Green;
		ColorMultiply = FLinearColor::White;
		Metallic = 0.0f;
		Specular = 0.0f;
		Roughness = 0.0f;
	}
};

/**
 * 六边形网格渲染组件
 * 基于UPrimitiveComponent实现，支持单个六棱柱或网格六棱柱渲染
 * 支持半径、高度、材质的参数控制，编辑器实时更新
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WANDERINGTILES_API UHexGridPrimitiveComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHexGridPrimitiveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// 渲染模式选择
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "渲染设置", meta = (DisplayName = "渲染模式"))
	EHexGridRenderMode RenderMode = EHexGridRenderMode::SingleHexagon;

	// 六棱柱半径（从中心到顶点的距离，单位：cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "六棱柱设置", meta = (DisplayName = "半径", ClampMin = "1.0", ClampMax = "10000.0", UIMin = "10.0", UIMax = "1000.0"))
	float Radius = 100.0f;

	// 六棱柱高度（单位：cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "六棱柱设置", meta = (DisplayName = "高度", ClampMin = "1.0", ClampMax = "10000.0", UIMin = "10.0", UIMax = "500.0"))
	float Height = 50.0f;
 
	// 六棱柱材质
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "六棱柱设置", meta = (DisplayName = "材质"))
	UMaterialInterface* Material = nullptr;

	// 网格尺寸设置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格设置", meta = (DisplayName = "X方向数量", ClampMin = "1", ClampMax = "50"))
	int32 GridSizeX = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格设置", meta = (DisplayName = "Y方向数量", ClampMin = "1", ClampMax = "50"))
	int32 GridSizeY = 4;

	// 网格间隙设置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "网格设置", meta = (DisplayName = "网格间隙", ClampMin = "0.0", ClampMax = "100.0"))
	float GridGap = 10.0f;

	// 材质实例化
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "材质设置", meta = (DisplayName = "基础材质"))
	UMaterialInterface* BaseMaterial = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* MaterialInstance = nullptr;

	// 是否显示线框（调试用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试设置", meta = (DisplayName = "显示线框"))
	bool bShowWireframe = false;

	// 调试绘制属性
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试设置", meta = (DisplayName = "显示包围盒"))
	bool bShowDebugBounds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试设置", meta = (DisplayName = "显示网格信息"))
	bool bShowDebugGrid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试设置", meta = (DisplayName = "包围盒颜色"))
	FColor DebugBoundsColor = FColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试设置", meta = (DisplayName = "网格颜色"))
	FColor DebugGridColor = FColor::Green;

	// 地块类型配置数组
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块管理", meta = (DisplayName = "地块类型配置"))
	TArray<FHexTileTypeConfig> TileTypeConfigs;

	// 当前所有地块的类型（网格模式下使用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块管理", meta = (DisplayName = "地块类型映射"))
	TMap<FIntPoint, EHexTileType> TileTypeMap;

	// 默认地块类型
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块管理", meta = (DisplayName = "默认地块类型"))
	EHexTileType DefaultTileType = EHexTileType::Grass;

	// 随机种子
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块管理", meta = (DisplayName = "随机种子"))
	int32 RandomSeed = 12345;

	// 可用的地块类型（用于随机生成）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地块管理", meta = (DisplayName = "可用地块类型"))
	TArray<EHexTileType> AvailableTileTypes;

public:
	// 设置渲染模式
	UFUNCTION(BlueprintCallable, Category = "渲染设置")
	void SetRenderMode(EHexGridRenderMode NewRenderMode);

	// 设置半径并重新生成几何体
	UFUNCTION(BlueprintCallable, Category = "六棱柱设置")
	void SetRadius(float NewRadius);

	// 设置高度并重新生成几何体
	UFUNCTION(BlueprintCallable, Category = "六棱柱设置")
	void SetHeight(float NewHeight);

	// 设置材质
	UFUNCTION(BlueprintCallable, Category = "六棱柱设置")
	void SetHexMaterial(UMaterialInterface* NewMaterial);

	// 重新生成六棱柱几何体
	UFUNCTION(BlueprintCallable, Category = "六棱柱设置")
	void RegenerateHexagon();

	// 获取渲染模式
	UFUNCTION(BlueprintPure, Category = "渲染设置")
	EHexGridRenderMode GetRenderMode() const { return RenderMode; }

	// 获取半径
	UFUNCTION(BlueprintPure, Category = "六棱柱设置")
	float GetRadius() const { return Radius; }

	// 获取高度
	UFUNCTION(BlueprintPure, Category = "六棱柱设置")
	float GetHeight() const { return Height; }

	// 获取六边形顶点的世界坐标
	UFUNCTION(BlueprintPure, Category = "六棱柱设置")
	TArray<FVector> GetHexagonVerticesWorld() const;

	// 切换线框显示
	UFUNCTION(BlueprintCallable, Category = "调试设置")
	void ToggleWireframe();

	// 调试绘制控制方法
	UFUNCTION(BlueprintCallable, Category = "调试设置", CallInEditor)
	void ToggleDebugBounds();

	UFUNCTION(BlueprintCallable, Category = "调试设置", CallInEditor)
	void ToggleDebugGrid();

	UFUNCTION(BlueprintCallable, Category = "调试设置", CallInEditor)
	void EnableDebugDrawing();

	UFUNCTION(BlueprintCallable, Category = "调试设置", CallInEditor)
	void DisableDebugDrawing();

	// 网格管理方法
	UFUNCTION(BlueprintCallable, Category = "网格管理")
	void SetGridSize(int32 NewSizeX, int32 NewSizeY);

	UFUNCTION(BlueprintCallable, Category = "网格管理")
	void SetGridGap(float NewGap);

	// 材质管理方法
	UFUNCTION(BlueprintCallable, Category = "材质管理")
	void SetBaseMaterial(UMaterialInterface* NewMaterial);

	// UFUNCTION(BlueprintCallable, Category = "材质管理")
	// void UpdateMaterialParameters();

	// 获取网格信息
	UFUNCTION(BlueprintPure, Category = "网格信息")
	int32 GetGridSizeX() const { return GridSizeX; }

	UFUNCTION(BlueprintPure, Category = "网格信息")
	int32 GetGridSizeY() const { return GridSizeY; }

	UFUNCTION(BlueprintPure, Category = "网格信息")
	int32 GetTotalTileCount() const { return GridSizeX * GridSizeY; }

	// 地块管理方法
	UFUNCTION(BlueprintCallable, Category = "地块管理")
	void SetTileType(FIntPoint GridPosition, EHexTileType NewTileType);

	UFUNCTION(BlueprintCallable, Category = "地块管理")
	EHexTileType GetTileType(FIntPoint GridPosition) const;

	UFUNCTION(BlueprintCallable, Category = "地块管理")
	void SetAllTilesType(EHexTileType TileType);

	UFUNCTION(BlueprintCallable, Category = "地块管理")
	void UpdateTileTypeConfig(EHexTileType TileType, const FHexTileTypeConfig& Config);

	UFUNCTION(BlueprintCallable, Category = "地块管理")
	FHexTileTypeConfig GetTileTypeConfig(EHexTileType TileType) const;


	// 获取所有地块类型配置
	UFUNCTION(BlueprintPure, Category = "地块管理")
	TArray<FHexTileTypeConfig> GetAllTileTypeConfigs() const { return TileTypeConfigs; }

	// 获取随机种子
	UFUNCTION(BlueprintPure, Category = "地块管理")
	int32 GetRandomSeed() const { return RandomSeed; }

	// 获取可用地块类型
	UFUNCTION(BlueprintPure, Category = "地块管理")
	TArray<EHexTileType> GetAvailableTileTypes() const { return AvailableTileTypes; }

	// 为指定位置生成随机地块类型
	UFUNCTION(BlueprintCallable, Category = "地块管理")
	EHexTileType GenerateRandomTileTypeForPosition(FIntPoint GridPosition);

	// 简化版的随机地块类型生成（仅草地和水域）
	UFUNCTION(BlueprintCallable, Category = "地块管理")
	EHexTileType GenerateSimpleRandomTileTypeForPosition(FIntPoint GridPosition);

	// 材质管理辅助方法
	// 检查是否应该使用动态材质实例
	UFUNCTION(BlueprintPure, Category = "材质管理")
	bool ShouldUseMaterialInstance() const { return BaseMaterial != nullptr; }

	// 同步着色器参数（原UpdateMaterialParameters重命名）
	UFUNCTION(BlueprintCallable, Category = "材质管理")
	void SyncShaderParameters();

	// 更新材质参数（新增）
	UFUNCTION(BlueprintCallable, Category = "材质管理")
	void UpdateMaterialParameters();

	// 获取当前使用的材质
	UFUNCTION(BlueprintPure, Category = "材质管理")
	UMaterialInterface* GetCurrentMaterial() const;

	// 验证材质实例是否有效
	UFUNCTION(BlueprintPure, Category = "材质管理")
	bool IsMaterialInstanceValid() const;

	// 获取材质实例状态信息
	UFUNCTION(BlueprintPure, Category = "材质管理")
	FString GetMaterialInstanceStatus() const;

	// 验证所有材质实例是否有效
	UFUNCTION(BlueprintPure, Category = "材质管理")
	bool ValidateTileTypeMaterialInstances() const;

	// 获取地块类型材质实例（供场景代理使用）
	const TMap<EHexTileType, UMaterialInstanceDynamic*>& GetTileTypeMaterialInstances() const 
	{ 
		return TileTypeMaterialInstances; 
	}

	// 获取地块类型分组数据（供场景代理使用）
	const TMap<EHexTileType, TArray<FVector>>& GetTileTypeVertices() const { return TileTypeVertices; }
	const TMap<EHexTileType, TArray<int32>>& GetTileTypeTriangles() const { return TileTypeTriangles; }
	const TMap<EHexTileType, TArray<FVector>>& GetTileTypeNormals() const { return TileTypeNormals; }
	const TMap<EHexTileType, TArray<FVector2D>>& GetTileTypeUVs() const { return TileTypeUVs; }
	const TMap<EHexTileType, TArray<FLinearColor>>& GetTileTypeVertexColors() const { return TileTypeVertexColors; }
	const TMap<EHexTileType, TArray<FProcMeshTangent>>& GetTileTypeTangents() const { return TileTypeTangents; }

	// 策略A：按地块类型分组的材质和网格管理
	UFUNCTION(BlueprintCallable, Category = "地块管理")
	void CreateTileTypeMaterialInstances();
	
	UFUNCTION(BlueprintCallable, Category = "地块管理")
	void GenerateTileTypeGroupedMesh();
	
	// 应用所有地块类型配置到材质实例
	UFUNCTION(BlueprintCallable, Category = "地块管理", CallInEditor)
	void ApplyTileTypeConfigsToMaterials();

	// 生成单个六棱柱网格数据（用于分组）
	void GenerateSingleHexagonMeshData(
		FIntPoint GridPos, 
		TArray<FVector>& OutVertices, 
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FVector2D>& OutUVs,
		TArray<FLinearColor>& OutVertexColors,
		TArray<FProcMeshTangent>& OutTangents);

protected:
	// 组件初始化时调用
	virtual void OnRegister() override;

	// 游戏开始时调用
	virtual void BeginPlay() override;

	// 属性变更回调
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// 创建场景代理
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	// 获取材质
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;

	// 获取材质数量
	virtual int32 GetNumMaterials() const override;

	// 获取使用的材质列表
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

	// 获取边界框
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	// 确保总是创建渲染状态
	virtual bool ShouldCreateRenderState() const override
	{
		return true; // 确保总是创建渲染状态
	}

	// 确保在非编辑器模式下也渲染
	virtual bool IsEditorOnly() const override
	{
		return false; // 确保在非编辑器模式下也渲染
	}


private:
	// 生成六棱柱网格数据
	void GenerateHexagonMesh();

	// 生成单个六棱柱网格
	void GenerateSingleHexagonMesh();

	// 生成网格六棱柱
	void GenerateHexGridMesh();

	// 生成单个六棱柱网格（带世界偏移）
	void GenerateSingleHexagonMesh(FIntPoint GridPos, const FVector& WorldOffset);

	// 计算六边形顶点位置（本地坐标）
	FVector CalculateHexagonVertex(int32 VertexIndex, float InRadius, float Z) const;

	// 计算六棱柱世界位置
	FVector CalculateHexagonWorldPosition(FIntPoint GridPos) const;

	// 公共方法供SceneProxy使用
	UFUNCTION(BlueprintPure, Category = "六棱柱设置")
	FVector GetHexagonVertex(int32 VertexIndex, float InRadius, float Z) const { return CalculateHexagonVertex(VertexIndex, InRadius, Z); }

	UFUNCTION(BlueprintPure, Category = "六棱柱设置")
	FVector GetHexagonWorldPosition(FIntPoint GridPos) const { return CalculateHexagonWorldPosition(GridPos); }

	// 计算六棱柱法线
	FVector CalculateHexagonNormal(int32 VertexIndex) const;

	// 计算六棱柱UV坐标
	FVector2D CalculateHexagonUV(int32 VertexIndex) const;

	// 计算六棱柱切线
	FProcMeshTangent CalculateHexagonTangent(int32 VertexIndex) const;

	// 创建材质实例
	void CreateMaterialInstance();

	// 更新边界框
	void UpdateBounds();

	// 初始化默认地块类型配置
	void InitializeDefaultTileTypeConfigs();

	// 材质设置处理方法
	void HandleMaterialSetup();
	
	// 网格模式材质处理
	void HandleGridModeMaterial();
	
	// 单个模式材质处理
	void HandleSingleModeMaterial();
	
	// 组件状态验证
	void ValidateComponentState();

	// 友元类声明，允许SceneProxy访问私有成员
	friend class FHexGridPrimitiveSceneProxy;

	// 网格数据
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	// 边界框缓存
	FBoxSphereBounds CachedBounds;

	// 策略A：按地块类型分组的材质实例
	UPROPERTY()
	TMap<EHexTileType, UMaterialInstanceDynamic*> TileTypeMaterialInstances;
	
	// 按地块类型分组的网格数据
	TMap<EHexTileType, TArray<FVector>> TileTypeVertices;
	TMap<EHexTileType, TArray<int32>> TileTypeTriangles;
	TMap<EHexTileType, TArray<FVector>> TileTypeNormals;
	TMap<EHexTileType, TArray<FVector2D>> TileTypeUVs;
	TMap<EHexTileType, TArray<FLinearColor>> TileTypeVertexColors;
	TMap<EHexTileType, TArray<FProcMeshTangent>> TileTypeTangents;
};
