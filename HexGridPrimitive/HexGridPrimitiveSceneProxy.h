// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "HexGridPrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"

/**
 * 六边形网格场景代理
 * 负责渲染单个六棱柱或网格六棱柱的几何体
 */
class WANDERINGTILES_API FHexGridPrimitiveSceneProxy : public FPrimitiveSceneProxy
{
public:
	FHexGridPrimitiveSceneProxy(const UHexGridPrimitiveComponent* InComponent);
	virtual ~FHexGridPrimitiveSceneProxy();

	// 获取动态网格元素
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, 
		const FSceneViewFamily& ViewFamily, 
		uint32 VisibilityMap, 
		FMeshElementCollector& Collector) const override;

	// 获取视图相关性
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	// 获取内存占用
	virtual SIZE_T GetTypeHash() const override;

	// 获取内存占用
	virtual uint32 GetMemoryFootprint() const override;

private:
	// 组件引用
	const UHexGridPrimitiveComponent* Component;

	// 几何数据
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	// 材质
	UMaterialInterface* Material;

	// 边界框
	FBoxSphereBounds Bounds;

	// 是否显示线框
	bool bShowWireframe;
};
