// Fill out your copyright notice in the Description page of Project Settings.

#include "HexGridPrimitiveSceneProxy.h"
#include "HexGridPrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "LocalVertexFactory.h"
#include "DynamicMeshBuilder.h"
#include "Materials/Material.h"

FHexGridPrimitiveSceneProxy::FHexGridPrimitiveSceneProxy(const UHexGridPrimitiveComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, Component(InComponent)
	, bShowWireframe(InComponent->bShowWireframe)
{
	// 复制几何数据
	Vertices = InComponent->Vertices;
	Triangles = InComponent->Triangles;
	Normals = InComponent->Normals;
	UVs = InComponent->UVs;
	VertexColors = InComponent->VertexColors;
	Tangents = InComponent->Tangents;
	
	// 获取材质 - 确保材质有效
	Material = InComponent->GetMaterial(0);
	if (!Material || !Material->IsValidLowLevel())
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	
	// 确保材质被标记为使用
	if (Material)
	{
		Material->AddToRoot(); // 防止材质被垃圾回收
	}
	
	// 计算边界框
	Bounds = InComponent->CalcBounds(FTransform::Identity);
}

FHexGridPrimitiveSceneProxy::~FHexGridPrimitiveSceneProxy()
{
}

void FHexGridPrimitiveSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, 
	const FSceneViewFamily& ViewFamily, 
	uint32 VisibilityMap, 
	FMeshElementCollector& Collector) const
{
	// 如果没有几何数据，直接返回
	if (Vertices.Num() == 0 || Triangles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FHexGridPrimitiveSceneProxy: No geometry data to render"));
		return;
	}

	// 确保材质有效
	if (!Material || !Material->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("FHexGridPrimitiveSceneProxy: Invalid material"));
		return;
	}

	// 为每个视图创建网格批次
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			
			// 创建动态网格构建器
			FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
			
			// 添加顶点
			for (int32 i = 0; i < Vertices.Num(); i++)
			{
				FDynamicMeshVertex Vertex;
				Vertex.Position = FVector3f(Vertices[i]);
				Vertex.TextureCoordinate[0] = FVector2f(UVs.IsValidIndex(i) ? UVs[i] : FVector2D::ZeroVector);
				Vertex.Color = (VertexColors.IsValidIndex(i) ? VertexColors[i] : FLinearColor::White).ToFColor(true);
				Vertex.SetTangents(
					FVector3f(Tangents.IsValidIndex(i) ? Tangents[i].TangentX : FVector(1, 0, 0)),
					FVector3f(Tangents.IsValidIndex(i) ? Tangents[i].TangentX : FVector(0, 1, 0)), // 使用TangentX作为副切线
					FVector3f(Normals.IsValidIndex(i) ? Normals[i] : FVector(0, 0, 1))
				);
				MeshBuilder.AddVertex(Vertex);
			}
			
			// 添加三角形
			for (int32 i = 0; i < Triangles.Num(); i += 3)
			{
				if (i + 2 < Triangles.Num())
				{
					MeshBuilder.AddTriangle(Triangles[i], Triangles[i + 1], Triangles[i + 2]);
				}
			}
			
			// 获取材质渲染代理
			FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy();
			
			// 创建FDynamicMeshBuilderSettings
			FDynamicMeshBuilderSettings Settings;
			Settings.bDisableBackfaceCulling = false;
			Settings.bReceivesDecals = false;
			Settings.bUseSelectionOutline = false;

			// 使用正确的GetMesh调用
			MeshBuilder.GetMesh(
				FMatrix::Identity,           // LocalToWorld
				MaterialRenderProxy,         // MaterialRenderProxy
				SDPG_World,                  // DepthPriorityGroup
				Settings,                      // FDynamicMeshBuilderSettings
				nullptr,                     // FDynamicMeshDrawOffset* (可以为nullptr)
				0,                          // ViewIndex
				Collector,                  // FMeshElementCollector&
				FHitProxyId()               // HitProxyId
			);
		}
	}
}

FPrimitiveViewRelevance FHexGridPrimitiveSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

SIZE_T FHexGridPrimitiveSceneProxy::GetTypeHash() const
{
	static SIZE_T UniquePointer;
	return reinterpret_cast<SIZE_T>(&UniquePointer);
}

uint32 FHexGridPrimitiveSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}
