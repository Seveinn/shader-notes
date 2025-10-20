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
	, bShowDebugBounds(InComponent->bShowDebugBounds)
	, bShowDebugGrid(InComponent->bShowDebugGrid)
	, DebugBoundsColor(InComponent->DebugBoundsColor)
	, DebugGridColor(InComponent->DebugGridColor)
{
	// 策略A：复制按地块类型分组的几何数据
	TileTypeVertices = InComponent->GetTileTypeVertices();
	TileTypeTriangles = InComponent->GetTileTypeTriangles();
	TileTypeNormals = InComponent->GetTileTypeNormals();
	TileTypeUVs = InComponent->GetTileTypeUVs();
	TileTypeVertexColors = InComponent->GetTileTypeVertexColors();
	TileTypeTangents = InComponent->GetTileTypeTangents();
	
	// 策略A：复制按地块类型分组的材质
	for (const auto& Pair : InComponent->GetTileTypeMaterialInstances())
	{
		if (Pair.Value && Pair.Value->IsValidLowLevel())
		{
			TileTypeMaterials.Add(Pair.Key, Pair.Value);
			// 注意：不要在这里调用AddToRoot，因为材质实例已经在组件中管理
		}
	}
	
	// 调试日志：检查材质实例传递情况
	UE_LOG(LogTemp, Warning, TEXT("FHexGridPrimitiveSceneProxy: TileTypeMaterials count: %d"), TileTypeMaterials.Num());
	for (const auto& Pair : TileTypeMaterials)
	{
		UE_LOG(LogTemp, Warning, TEXT("  - %s: %s"), 
			   *UEnum::GetValueAsString(Pair.Key),
			   Pair.Value ? TEXT("Valid") : TEXT("Null"));
	}
	
	// 计算边界框
	Bounds = InComponent->CalcBounds(FTransform::Identity);
	
	// 调试日志：检查边界框
	UE_LOG(LogTemp, Warning, TEXT("FHexGridPrimitiveSceneProxy: Bounds - Origin: %s, Extent: %s"), 
		   *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString());
}

FHexGridPrimitiveSceneProxy::~FHexGridPrimitiveSceneProxy()
{
}

void FHexGridPrimitiveSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, 
	const FSceneViewFamily& ViewFamily, 
	uint32 VisibilityMap, 
	FMeshElementCollector& Collector) const
{
	// 调试日志：检查网格数据
	// UE_LOG(LogTemp, Warning, TEXT("FHexGridPrimitiveSceneProxy: GetDynamicMeshElements called - Processing %d tile types"), TileTypeVertices.Num());
	// for (const auto& Pair : TileTypeVertices)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("  - %s: %d vertices, %d triangles"), 
	// 		   *UEnum::GetValueAsString(Pair.Key),
	// 		   Pair.Value.Num(),
	// 		   TileTypeTriangles.Contains(Pair.Key) ? TileTypeTriangles[Pair.Key].Num() : 0);
	// }
	
	// 策略A：按地块类型分组渲染
	for (const auto& TileTypePair : TileTypeVertices)
	{
		EHexTileType TileType = TileTypePair.Key;
		const TArray<FVector>& TileVertices = TileTypePair.Value;
		const TArray<int32>& TileTriangles = TileTypeTriangles[TileType];
		const TArray<FVector>& TileNormals = TileTypeNormals[TileType];
		const TArray<FVector2D>& TileUVs = TileTypeUVs[TileType];
		const TArray<FLinearColor>& TileVertexColors = TileTypeVertexColors[TileType];
		const TArray<FProcMeshTangent>& TileTangents = TileTypeTangents[TileType];
		
		// 如果没有几何数据，跳过
		if (TileVertices.Num() == 0 || TileTriangles.Num() == 0)
		{
			continue;
		}
		
		// 获取对应的材质
		UMaterialInterface* Material = TileTypeMaterials.Contains(TileType) ? TileTypeMaterials[TileType] : nullptr;
		if (!Material || !Material->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Warning, TEXT("FHexGridPrimitiveSceneProxy: Invalid material for tile type %s"), 
				   *UEnum::GetValueAsString(TileType));
			continue;
		}

		// 添加调试日志：材质有效
		// UE_LOG(LogTemp, Warning, TEXT("Material for %s is valid, proceeding with rendering"), 
		// 	   *UEnum::GetValueAsString(TileType));
		
		// 为每个视图创建网格批次
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				
				// 创建动态网格构建器
				FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
				
				// 添加顶点
				for (int32 i = 0; i < TileVertices.Num(); i++)
				{
					FDynamicMeshVertex Vertex;
					Vertex.Position = FVector3f(TileVertices[i]);
					Vertex.TextureCoordinate[0] = FVector2f(TileUVs.IsValidIndex(i) ? TileUVs[i] : FVector2D::ZeroVector);
					Vertex.Color = (TileVertexColors.IsValidIndex(i) ? TileVertexColors[i] : FLinearColor::White).ToFColor(true);
					Vertex.SetTangents(
						FVector3f(TileTangents.IsValidIndex(i) ? TileTangents[i].TangentX : FVector(1, 0, 0)),
						FVector3f(TileTangents.IsValidIndex(i) ? TileTangents[i].TangentX : FVector(0, 1, 0)), // 使用TangentX作为副切线
						FVector3f(TileNormals.IsValidIndex(i) ? TileNormals[i] : FVector(0, 0, 1))
					);
					MeshBuilder.AddVertex(Vertex);
				}
				
				// 添加三角形
				for (int32 i = 0; i < TileTriangles.Num(); i += 3)
				{
					if (i + 2 < TileTriangles.Num())
					{
						MeshBuilder.AddTriangle(TileTriangles[i], TileTriangles[i + 1], TileTriangles[i + 2]);
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
					ViewIndex,                  // ViewIndex
					Collector,                  // FMeshElementCollector&
					FHitProxyId()               // HitProxyId
				);
			}
		}
	}

	// 添加调试绘制
	if (bShowDebugBounds || bShowDebugGrid)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				
				if (bShowDebugBounds)
				{
					DrawDebugBounds(View, PDI);
				}
				
				if (bShowDebugGrid)
				{
					DrawDebugGrid(View, PDI);
				}
			}
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

// 绘制调试包围盒
void FHexGridPrimitiveSceneProxy::DrawDebugBounds(const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
	if (!PDI)
		return;

	// 绘制包围盒
	FVector Origin = Bounds.Origin;
	FVector Extent = Bounds.BoxExtent;
	
	// 绘制包围盒的8个顶点
	FVector Corners[8];
	Corners[0] = Origin + FVector(-Extent.X, -Extent.Y, -Extent.Z);
	Corners[1] = Origin + FVector( Extent.X, -Extent.Y, -Extent.Z);
	Corners[2] = Origin + FVector( Extent.X,  Extent.Y, -Extent.Z);
	Corners[3] = Origin + FVector(-Extent.X,  Extent.Y, -Extent.Z);
	Corners[4] = Origin + FVector(-Extent.X, -Extent.Y,  Extent.Z);
	Corners[5] = Origin + FVector( Extent.X, -Extent.Y,  Extent.Z);
	Corners[6] = Origin + FVector( Extent.X,  Extent.Y,  Extent.Z);
	Corners[7] = Origin + FVector(-Extent.X,  Extent.Y,  Extent.Z);
	
	// 绘制包围盒的12条边
	int32 Edges[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // 底面
		{4,5}, {5,6}, {6,7}, {7,4}, // 顶面
		{0,4}, {1,5}, {2,6}, {3,7}  // 连接边
	};
	
	for (int32 i = 0; i < 12; i++)
	{
		PDI->DrawLine(Corners[Edges[i][0]], Corners[Edges[i][1]], DebugBoundsColor, SDPG_World, 2.0f);
	}
	
	// 绘制包围球
	DrawWireSphere(PDI, Origin, Bounds.SphereRadius, 16, DebugBoundsColor, SDPG_World);
}

// 绘制调试网格
void FHexGridPrimitiveSceneProxy::DrawDebugGrid(const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
	if (!PDI || !Component)
		return;

	// 只在网格模式下绘制
	if (Component->GetRenderMode() != EHexGridRenderMode::HexGrid)
		return;

	// 获取组件变换
	FTransform ComponentTransform = Component->GetComponentTransform();
	
	// 绘制网格信息
	for (int32 Y = 0; Y < Component->GetGridSizeY(); Y++)
	{
		for (int32 X = 0; X < Component->GetGridSizeX(); X++)
		{
			FIntPoint GridPos(X, Y);
			FVector LocalPos = Component->GetHexagonWorldPosition(GridPos);
			FVector WorldPos = ComponentTransform.TransformPosition(LocalPos);
			
			// 绘制六边形底面
			for (int32 i = 0; i < 6; i++)
			{
				FVector Vertex1 = Component->GetHexagonVertex(i, Component->GetRadius(), 0.0f);
				FVector Vertex2 = Component->GetHexagonVertex((i + 1) % 6, Component->GetRadius(), 0.0f);
				FVector WorldVertex1 = ComponentTransform.TransformPosition(LocalPos + Vertex1);
				FVector WorldVertex2 = ComponentTransform.TransformPosition(LocalPos + Vertex2);
				
				PDI->DrawLine(WorldVertex1, WorldVertex2, DebugGridColor, SDPG_World, 1.0f);
			}
			
			// 绘制六边形顶面
			for (int32 i = 0; i < 6; i++)
			{
				FVector Vertex1 = Component->GetHexagonVertex(i, Component->GetRadius(), Component->GetHeight());
				FVector Vertex2 = Component->GetHexagonVertex((i + 1) % 6, Component->GetRadius(), Component->GetHeight());
				FVector WorldVertex1 = ComponentTransform.TransformPosition(LocalPos + Vertex1);
				FVector WorldVertex2 = ComponentTransform.TransformPosition(LocalPos + Vertex2);
				
				PDI->DrawLine(WorldVertex1, WorldVertex2, DebugGridColor, SDPG_World, 1.0f);
			}
			
			// 绘制连接线
			for (int32 i = 0; i < 6; i++)
			{
				FVector BottomVertex = Component->GetHexagonVertex(i, Component->GetRadius(), 0.0f);
				FVector TopVertex = Component->GetHexagonVertex(i, Component->GetRadius(), Component->GetHeight());
				FVector WorldBottom = ComponentTransform.TransformPosition(LocalPos + BottomVertex);
				FVector WorldTop = ComponentTransform.TransformPosition(LocalPos + TopVertex);
				
				PDI->DrawLine(WorldBottom, WorldTop, DebugGridColor, SDPG_World, 1.0f);
			}
		}
	}
}
