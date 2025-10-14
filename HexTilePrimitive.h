// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"  // 添加这个头文件以获取FProcMeshTangent
#include "HexTilePrimitive.generated.h"

// 前向声明
class FHexTilePrimitiveSceneProxy;

/**
 * 单个六棱柱渲染组件
 * 基于UPrimitiveComponent实现，坐标原点位于底面中心
 * 支持半径、高度、材质的参数控制，编辑器实时更新
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WANDERINGTILES_API UHexTilePrimitive : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHexTilePrimitive(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// 六棱柱半径（从中心到顶点的距离，单位：cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "六棱柱设置", meta = (DisplayName = "半径", ClampMin = "1.0", ClampMax = "10000.0", UIMin = "10.0", UIMax = "1000.0"))
	float Radius = 100.0f;

	// 六棱柱高度（单位：cm）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "六棱柱设置", meta = (DisplayName = "高度", ClampMin = "1.0", ClampMax = "10000.0", UIMin = "10.0", UIMax = "500.0"))
	float Height = 50.0f;

	// 六棱柱材质
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "六棱柱设置", meta = (DisplayName = "材质"))
	UMaterialInterface* Material = nullptr;

	// 是否显示线框（调试用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "调试设置", meta = (DisplayName = "显示线框"))
	bool bShowWireframe = false;

public:
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

private:
	// 生成六棱柱网格数据
	void GenerateHexagonMesh();

	// 计算六边形顶点位置（本地坐标）
	FVector CalculateHexagonVertex(int32 VertexIndex, float InRadius, float Z) const;

	// 友元类声明，允许SceneProxy访问私有成员
	friend class FHexTilePrimitiveSceneProxy;

	// 网格数据
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	// 边界框缓存
	FBoxSphereBounds CachedBounds;
};
