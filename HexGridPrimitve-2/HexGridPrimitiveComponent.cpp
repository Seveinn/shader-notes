// Fill out your copyright notice in the Description page of Project Settings.

#include "HexGridPrimitiveComponent.h"
#include "HexGridPrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"

UHexGridPrimitiveComponent::UHexGridPrimitiveComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 设置组件默认属性
	PrimaryComponentTick.bCanEverTick = false;
	
	// 设置碰撞
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);

	// 启用阴影
	bCastDynamicShadow = true;
	CastShadow = true;
	
	// 初始化网格尺寸
	GridSizeX = 5;
	GridSizeY = 4;
	GridGap = 10.0f;

	// 初始化随机地块生成相关变量
	RandomSeed = 12345;
	AvailableTileTypes.Empty();
	AvailableTileTypes.Add(EHexTileType::Grass);
	AvailableTileTypes.Add(EHexTileType::Water);

	// 初始化默认地块类型配置
	InitializeDefaultTileTypeConfigs();

	// 启用编辑器调试绘制
	bShowDebugBounds = true;
	bShowDebugGrid = false;
	DebugBoundsColor = FColor::Red;
	DebugGridColor = FColor::Green;
}

void UHexGridPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
	
	// 调试日志：组件注册
	UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: OnRegister called, generating mesh"));
	
	// 1. 验证基础参数
	if (Radius <= 0.0f || Height <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: Invalid dimensions - Radius: %f, Height: %f"), Radius, Height);
		return;
	}
	
	// 2. 处理材质设置（在网格生成之前）
	HandleMaterialSetup();
	
	// 3. 组件注册时生成网格
	GenerateHexagonMesh();
	
	// 4. 验证最终状态
	ValidateComponentState();
	
	// 5. 确保渲染状态被标记为需要更新
	MarkRenderStateDirty();
}

void UHexGridPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// 调试日志：检查材质状态
	UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: BeginPlay - MaterialInstance: %s, BaseMaterial: %s, Material: %s"), 
		   MaterialInstance ? TEXT("Valid") : TEXT("Null"),
		   BaseMaterial ? TEXT("Valid") : TEXT("Null"),
		   Material ? TEXT("Valid") : TEXT("Null"));
	
	// 优先创建材质实例
	// if (BaseMaterial)
	// {
	// 	CreateMaterialInstance();
	// 	// 确保材质正确应用
	// 	if (MaterialInstance)
	// 	{
	// 		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Material instance created successfully in BeginPlay"));
	// 	}
	// }
	// // 如果没有基础材质，使用传统材质
	// else if (Material)
	// {
	// 	SetMaterial(0, Material);
	// 	UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Using traditional material in BeginPlay"));
	// }
}

void UHexGridPrimitiveComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 检查网格尺寸变更
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, GridSizeX) ||
		PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, GridSizeY))
	{
		GenerateHexagonMesh();
		// 添加：同步着色器参数
		SyncShaderParameters();
		MarkRenderStateDirty();
	}
	// 检查网格间隙变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, GridGap))
	{
		GenerateHexagonMesh();
		// 添加：同步着色器参数
		SyncShaderParameters();
		MarkRenderStateDirty();
	}
	// 检查基础材质变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, BaseMaterial))
	{
		CreateMaterialInstance();
		// 采用与六棱柱材质相同的简单刷新方案
		MarkRenderStateDirty();
	}
	// 检查传统材质变更（仅在BaseMaterial为空时生效）
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, Material))
	{
		// 只有在没有基础材质时才使用传统材质
		if (!BaseMaterial)
		{
			if (Material)
			{
				SetMaterial(0, Material);
			}
			else
			{
				SetMaterial(0, nullptr);
			}
			MarkRenderStateDirty();
		}
	}
	// 检查地块类型配置变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, TileTypeConfigs))
	{
		// 重新应用材质参数
		ApplyTileTypeConfigsToMaterials();
		MarkRenderStateDirty();
	}
	// 检查渲染模式变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, RenderMode))
	{
		// 重新生成网格
		GenerateHexagonMesh();
		MarkRenderStateDirty();
	}
	// 检查其他需要重新生成网格的属性
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, Radius) ||
			 PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, Height))
	{
		// 重新生成网格
		GenerateHexagonMesh();
		// 添加：同步着色器参数
		SyncShaderParameters();
		MarkRenderStateDirty();
	}
	// 检查线框属性变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, bShowWireframe))
	{
		MarkRenderStateDirty();
	}
	// 检查地块类型配置变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, TileTypeConfigs))
	{
		// 重新应用所有地块类型的材质参数
		ApplyTileTypeConfigsToMaterials();
		MarkRenderStateDirty();
	}
	// 检查默认地块类型变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, DefaultTileType))
	{
		// 重新应用所有地块类型的材质参数
		ApplyTileTypeConfigsToMaterials();
		MarkRenderStateDirty();
	}
	// 检查地块配置中的颜色参数变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, BaseColor) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, ColorMultiply) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, Metallic) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, Specular) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, Roughness))
	{
		// 重新应用所有地块类型的材质参数
		ApplyTileTypeConfigsToMaterials();
		MarkRenderStateDirty();
	}
}

void UHexGridPrimitiveComponent::SetRenderMode(EHexGridRenderMode NewRenderMode)
{
	if (RenderMode != NewRenderMode)
	{
		RenderMode = NewRenderMode;
		RegenerateHexagon();
	}
}

void UHexGridPrimitiveComponent::SetRadius(float NewRadius)
{
	if (NewRadius > 0.0f && NewRadius != Radius)
	{
		Radius = NewRadius;
		RegenerateHexagon();
		
		// 添加：同步着色器参数
		SyncShaderParameters();
	}
}

void UHexGridPrimitiveComponent::SetHeight(float NewHeight)
{
	if (NewHeight > 0.0f && NewHeight != Height)
	{
		Height = NewHeight;
		RegenerateHexagon();
		
		// 添加：同步着色器参数
		SyncShaderParameters();
	}
}

void UHexGridPrimitiveComponent::SetHexMaterial(UMaterialInterface* NewMaterial)
{
	if (Material != NewMaterial)
	{
		Material = NewMaterial;
		if (Material)
		{
			SetMaterial(0, Material);
		}
		else
		{
			SetMaterial(0, nullptr);
		}
		MarkRenderStateDirty();
	}
}

void UHexGridPrimitiveComponent::RegenerateHexagon()
{
	GenerateHexagonMesh();
	MarkRenderStateDirty();
}

TArray<FVector> UHexGridPrimitiveComponent::GetHexagonVerticesWorld() const
{
	TArray<FVector> WorldVertices;
	
	for (int32 i = 0; i < 6; i++)
	{
		FVector LocalVertex = CalculateHexagonVertex(i, Radius, 0.0f);
		FVector WorldVertex = GetComponentTransform().TransformPosition(LocalVertex);
		WorldVertices.Add(WorldVertex);
	}
	
	return WorldVertices;
}

void UHexGridPrimitiveComponent::ToggleWireframe()
{
	bShowWireframe = !bShowWireframe;
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UHexGridPrimitiveComponent::CreateSceneProxy()
{
	return new FHexGridPrimitiveSceneProxy(this);
}

UMaterialInterface* UHexGridPrimitiveComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0)
	{
		// 优先使用动态材质实例
		if (MaterialInstance)
		{
			return MaterialInstance;
		}
		// 如果没有材质实例，使用基础材质
		else if (BaseMaterial)
		{
			return BaseMaterial;
		}
		// 最后使用传统材质
		else if (Material)
		{
			return Material;
		}
	}
	return nullptr;
}

int32 UHexGridPrimitiveComponent::GetNumMaterials() const
{
	return 1;
}

void UHexGridPrimitiveComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	OutMaterials.Empty();
	
	// 根据渲染模式处理材质注册
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		// 网格模式：注册所有地块类型的材质实例
		for (const auto& Pair : TileTypeMaterialInstances)
		{
			if (Pair.Value && Pair.Value->IsValidLowLevel())
			{
				OutMaterials.Add(Pair.Value);
			}
		}
		
		// 如果没有地块类型材质实例，回退到基础材质
		if (OutMaterials.Num() == 0)
		{
			if (BaseMaterial)
			{
				OutMaterials.Add(BaseMaterial);
			}
			else if (Material)
			{
				OutMaterials.Add(Material);
			}
		}
	}
	else
	{
		// 单个六棱柱模式：使用传统材质管理
		// 优先使用动态材质实例
		if (MaterialInstance)
		{
			OutMaterials.Add(MaterialInstance);
		}
		// 如果没有材质实例，使用基础材质
		else if (BaseMaterial)
		{
			OutMaterials.Add(BaseMaterial);
		}
		// 最后使用传统材质
		else if (Material)
		{
			OutMaterials.Add(Material);
		}
	}
}

FBoxSphereBounds UHexGridPrimitiveComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		// 确保缓存边界框有效
		if (CachedBounds.SphereRadius <= 0.0f)
		{
			// 如果缓存边界框无效，重新计算
			FVector GridExtent = FVector(
				(GridSizeX * (Radius * 1.5f + GridGap)) * 0.5f,
				(GridSizeY * (Radius * FMath::Sqrt(3.0f) + GridGap)) * 0.5f,
				Height * 0.5f
			);
			FBox LocalBounds = FBox(-GridExtent, GridExtent);
			FBoxSphereBounds TempBounds = FBoxSphereBounds(LocalBounds);
			return TempBounds.TransformBy(LocalToWorld);
		}
		return CachedBounds.TransformBy(LocalToWorld);
	}
	else
	{
		// 确保单个六棱柱的边界框计算正确
		FVector Extent = FVector(Radius, Radius, Height * 0.5f);
		FBox LocalBounds = FBox(-Extent, Extent);
		return FBoxSphereBounds(LocalBounds.TransformBy(LocalToWorld));
	}
}

FVector UHexGridPrimitiveComponent::CalculateHexagonVertex(int32 VertexIndex, float InRadius, float Z) const
{
	// 六边形顶点角度：0度为右侧，顺时针旋转
	// 角度序列：0°, 60°, 120°, 180°, 240°, 300°
	float AngleDeg = 60.0f * VertexIndex;
	float AngleRad = FMath::DegreesToRadians(AngleDeg);
	
	float X = InRadius * FMath::Cos(AngleRad);
	float Y = InRadius * FMath::Sin(AngleRad);
	
	return FVector(X, Y, Z);
}

void UHexGridPrimitiveComponent::GenerateHexagonMesh()
{
	// 清除现有网格数据
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UVs.Empty();
	VertexColors.Empty();
	Tangents.Empty();
	
	// 验证参数
	if (Radius <= 0.0f || Height <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Invalid dimensions - Radius: %f, Height: %f"), Radius, Height);
		return;
	}
	
	// 根据渲染模式生成不同的网格
	switch (RenderMode)
	{
		case EHexGridRenderMode::SingleHexagon:
			GenerateSingleHexagonMesh();
			break;
		case EHexGridRenderMode::HexGrid:
			GenerateHexGridMesh();
			break;
		default:
			GenerateSingleHexagonMesh();
			break;
	}
	
	// 更新边界框缓存
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		UpdateBounds();
	}
	else
	{
		CachedBounds = CalcBounds(FTransform::Identity);
	}
	
	// 确保边界框更新
	UpdateBounds();
	
	// 强制标记渲染状态需要更新
	MarkRenderStateDirty();
	
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Generated %d vertices, %d triangles for %dx%d grid"), 
			   Vertices.Num(), Triangles.Num(), GridSizeX, GridSizeY);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Generated %d vertices, %d triangles"), 
			   Vertices.Num(), Triangles.Num());
	}
}

void UHexGridPrimitiveComponent::GenerateSingleHexagonMesh()
{
	// 底面中心点
	Vertices.Add(FVector(0, 0, 0));
	UVs.Add(FVector2D(0.5f, 0.5f));
	Normals.Add(FVector(0, 0, -1));
	VertexColors.Add(FLinearColor::White);
	Tangents.Add(FProcMeshTangent(1, 0, 0));
	
	// 底面6个顶点
	for (int32 i = 0; i < 6; i++)
	{
		Vertices.Add(CalculateHexagonVertex(i, Radius, 0.0f));
		UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(FMath::DegreesToRadians(60.0f * i)), 
						  0.5f + 0.5f * FMath::Sin(FMath::DegreesToRadians(60.0f * i))));
		Normals.Add(FVector(0, 0, -1));
		VertexColors.Add(FLinearColor::White);
		Tangents.Add(FProcMeshTangent(1, 0, 0));
	}
	
	// 顶面中心点
	Vertices.Add(FVector(0, 0, Height));
	UVs.Add(FVector2D(0.5f, 0.5f));
	Normals.Add(FVector(0, 0, 1));
	VertexColors.Add(FLinearColor::White);
	Tangents.Add(FProcMeshTangent(1, 0, 0));
	
	// 顶面6个顶点
	for (int32 i = 0; i < 6; i++)
	{
		Vertices.Add(CalculateHexagonVertex(i, Radius, Height));
		UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(FMath::DegreesToRadians(60.0f * i)), 
						  0.5f + 0.5f * FMath::Sin(FMath::DegreesToRadians(60.0f * i))));
		Normals.Add(FVector(0, 0, 1));
		VertexColors.Add(FLinearColor::White);
		Tangents.Add(FProcMeshTangent(1, 0, 0));
	}
	
	// 生成三角形
	// 底面三角形（6个扇形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		Triangles.Add(0); // 中心点
		Triangles.Add(i + 1);
		Triangles.Add(Next + 1);
	}
	
	// 顶面三角形（6个扇形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		Triangles.Add(7); // 顶面中心点
		Triangles.Add(7 + Next + 1); // 下一个顶点
		Triangles.Add(7 + i + 1);     // 当前顶点
	}
	
	// 侧面（6个四边形，每个2个三角形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		int32 BottomCurrent = i + 1;
		int32 BottomNext = Next + 1;
		int32 TopCurrent = 7 + i + 1;
		int32 TopNext = 7 + Next + 1;
		
		// 计算侧面法线
		FVector SideNormal = FVector::CrossProduct(
			Vertices[TopCurrent] - Vertices[BottomCurrent],
			Vertices[BottomNext] - Vertices[BottomCurrent]
		).GetSafeNormal();
		
		// 更新侧面顶点法线
		Normals[BottomCurrent] = (Normals[BottomCurrent] + SideNormal).GetSafeNormal();
		Normals[BottomNext] = (Normals[BottomNext] + SideNormal).GetSafeNormal();
		Normals[TopCurrent] = (Normals[TopCurrent] + SideNormal).GetSafeNormal();
		Normals[TopNext] = (Normals[TopNext] + SideNormal).GetSafeNormal();
		
		// 三角形1
		Triangles.Add(BottomCurrent);
		Triangles.Add(TopNext);
		Triangles.Add(BottomNext);
		
		// 三角形2
		Triangles.Add(BottomCurrent);
		Triangles.Add(TopCurrent);
		Triangles.Add(TopNext);
	}
}

void UHexGridPrimitiveComponent::GenerateHexGridMesh()
{
	// 计算六边形间距
	float HexWidth = Radius * 2.0f;
	float HexHeight = Radius * FMath::Sqrt(3.0f);
	float HorizontalSpacing = HexWidth * 0.75f + GridGap;
	float VerticalSpacing = HexHeight + GridGap;
	
	int32 VertexOffset = 0;
	
	// 初始化随机种子（如果还没有设置）
	if (RandomSeed == 0)
	{
		RandomSeed = FMath::RandRange(1000, 9999);
	}
	FMath::RandInit(RandomSeed);
	
	// 设置默认可用类型（如果还没有设置）
	if (AvailableTileTypes.Num() == 0)
	{
		AvailableTileTypes.Add(EHexTileType::Grass);
		AvailableTileTypes.Add(EHexTileType::Water);
	}
	
	// 清空现有地块类型映射
	TileTypeMap.Empty();
	
	// 生成每个六棱柱并同时分配地块类型
	for (int32 Y = 0; Y < GridSizeY; Y++)
	{
		for (int32 X = 0; X < GridSizeX; X++)
		{
			FIntPoint GridPos(X, Y);
			FVector WorldOffset = CalculateHexagonWorldPosition(GridPos);
			
			// 在同一个循环中生成地块类型
			// EHexTileType RandomTileType = GenerateRandomTileTypeForPosition(GridPos);
			// 使用简化版的随机地块类型生成
			EHexTileType RandomTileType = GenerateSimpleRandomTileTypeForPosition(GridPos);

			TileTypeMap.Add(GridPos, RandomTileType);
			
			// 生成单个六棱柱
			GenerateSingleHexagonMesh(GridPos, WorldOffset);
			VertexOffset += 14; // 每个六棱柱14个顶点


			
			UE_LOG(LogTemp, Log, TEXT("Generated tile at (%d, %d) with type: %s"), 
				   X, Y, *UEnum::GetValueAsString(RandomTileType));
		}
	}
	
	// 策略A：创建地块类型材质实例并生成分组网格
	CreateTileTypeMaterialInstances();
	GenerateTileTypeGroupedMesh();
	
	// 验证材质实例是否有效
	if (!ValidateTileTypeMaterialInstances())
	{
		UE_LOG(LogTemp, Error, TEXT("Some tile type material instances are invalid"));
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Generated %d random tiles with %d different types in single pass"), 
		   TileTypeMap.Num(), AvailableTileTypes.Num());
}

void UHexGridPrimitiveComponent::GenerateSingleHexagonMesh(FIntPoint GridPos, const FVector& WorldOffset)
{
	int32 StartVertexIndex = Vertices.Num();
	
	// 底面中心点
	Vertices.Add(WorldOffset + FVector(0, 0, 0));
	UVs.Add(FVector2D(0.5f, 0.5f));
	Normals.Add(FVector(0, 0, -1));
	VertexColors.Add(FLinearColor::White);
	Tangents.Add(FProcMeshTangent(1, 0, 0));
	
	// 底面6个顶点
	for (int32 i = 0; i < 6; i++)
	{
		FVector LocalVertex = CalculateHexagonVertex(i, Radius, 0.0f);
		Vertices.Add(WorldOffset + LocalVertex);
		UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(FMath::DegreesToRadians(60.0f * i)), 
						  0.5f + 0.5f * FMath::Sin(FMath::DegreesToRadians(60.0f * i))));
		Normals.Add(FVector(0, 0, -1));
		VertexColors.Add(FLinearColor::White);
		Tangents.Add(FProcMeshTangent(1, 0, 0));
	}
	
	// 顶面中心点
	Vertices.Add(WorldOffset + FVector(0, 0, Height));
	UVs.Add(FVector2D(0.5f, 0.5f));
	Normals.Add(FVector(0, 0, 1));
	VertexColors.Add(FLinearColor::White);
	Tangents.Add(FProcMeshTangent(1, 0, 0));
	
	// 顶面6个顶点
	for (int32 i = 0; i < 6; i++)
	{
		FVector LocalVertex = CalculateHexagonVertex(i, Radius, Height);
		Vertices.Add(WorldOffset + LocalVertex);
		UVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(FMath::DegreesToRadians(60.0f * i)), 
						  0.5f + 0.5f * FMath::Sin(FMath::DegreesToRadians(60.0f * i))));
		Normals.Add(FVector(0, 0, 1));
		VertexColors.Add(FLinearColor::White);
		Tangents.Add(FProcMeshTangent(1, 0, 0));
	}
	
	// 生成三角形
	// 底面三角形（6个扇形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		Triangles.Add(StartVertexIndex + 0); // 中心点
		Triangles.Add(StartVertexIndex + i + 1);
		Triangles.Add(StartVertexIndex + Next + 1);
	}
	
	// 顶面三角形（6个扇形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		Triangles.Add(StartVertexIndex + 7); // 顶面中心点
		Triangles.Add(StartVertexIndex + 7 + Next + 1); // 下一个顶点
		Triangles.Add(StartVertexIndex + 7 + i + 1);     // 当前顶点
	}
	
	// 侧面（6个四边形，每个2个三角形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		int32 BottomCurrent = StartVertexIndex + i + 1;
		int32 BottomNext = StartVertexIndex + Next + 1;
		int32 TopCurrent = StartVertexIndex + 7 + i + 1;
		int32 TopNext = StartVertexIndex + 7 + Next + 1;
		
		// 计算侧面法线
		FVector SideNormal = FVector::CrossProduct(
			Vertices[TopCurrent] - Vertices[BottomCurrent],
			Vertices[BottomNext] - Vertices[BottomCurrent]
		).GetSafeNormal();
		
		// 更新侧面顶点法线
		Normals[BottomCurrent] = (Normals[BottomCurrent] + SideNormal).GetSafeNormal();
		Normals[BottomNext] = (Normals[BottomNext] + SideNormal).GetSafeNormal();
		Normals[TopCurrent] = (Normals[TopCurrent] + SideNormal).GetSafeNormal();
		Normals[TopNext] = (Normals[TopNext] + SideNormal).GetSafeNormal();
		
		// 三角形1
		Triangles.Add(BottomCurrent);
		Triangles.Add(TopNext);
		Triangles.Add(BottomNext);
		
		// 三角形2
		Triangles.Add(BottomCurrent);
		Triangles.Add(TopCurrent);
		Triangles.Add(TopNext);
	}
}

FVector UHexGridPrimitiveComponent::CalculateHexagonWorldPosition(FIntPoint GridPos) const
{
	// 计算六边形间距
	float HexWidth = Radius * 2.0f;
	float HexHeight = Radius * FMath::Sqrt(3.0f);
	float HorizontalSpacing = HexWidth * 0.75f + GridGap;
	float VerticalSpacing = HexHeight + GridGap;
	
	// 计算世界位置
	float WorldX = (GridPos.X - GridSizeX * 0.5f) * HorizontalSpacing;
	float WorldY = (GridPos.Y - GridSizeY * 0.5f) * VerticalSpacing + (GridPos.X % 2) * VerticalSpacing * 0.5f;
	
	return FVector(WorldX, WorldY, 0.0f);
}

void UHexGridPrimitiveComponent::CreateMaterialInstance()
{
	if (BaseMaterial)
	{
		// 验证基础材质有效性
		if (!BaseMaterial->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: BaseMaterial is invalid"));
			MaterialInstance = nullptr;
			SetMaterial(0, nullptr);
			return;
		}
		
		// 清理旧的材质实例
		if (MaterialInstance)
		{
			MaterialInstance = nullptr;
		}
		
		// 创建新的材质实例
		MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (MaterialInstance)
		{
			// 关键修复：将材质实例设置到组件
			SetMaterial(0, MaterialInstance);
			
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Material instance created and set successfully"));
			// 打印材质实例信息
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Material instance status: %s"), *GetMaterialInstanceStatus());
			// LogTemp: Warning: UHexGridPrimitiveComponent: Material instance status: MaterialInstance: Valid, BaseMaterial: M_Color
			
			// 同步着色器参数
			SyncShaderParameters();
			
			// 启用材质参数更新（取消注释并修复）
			UpdateMaterialParameters();
			
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Material parameters applied successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: Failed to create material instance from BaseMaterial: %s"), 
				*BaseMaterial->GetName());
			MaterialInstance = nullptr;
			SetMaterial(0, nullptr);
		}
	}
	else
	{
		MaterialInstance = nullptr;
		// 如果BaseMaterial为空，回退到传统材质
		if (Material)
		{
			SetMaterial(0, Material);
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Fallback to traditional material"));
		}
		else
		{
			SetMaterial(0, nullptr);
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: No material set"));
		}
	}
}

void UHexGridPrimitiveComponent::SyncShaderParameters()
{
	if (MaterialInstance)
	{
		// 同步几何体参数到着色器
		MaterialInstance->SetScalarParameterValue(TEXT("GridSizeX"), static_cast<float>(GridSizeX));
		MaterialInstance->SetScalarParameterValue(TEXT("GridSizeY"), static_cast<float>(GridSizeY));
		MaterialInstance->SetScalarParameterValue(TEXT("Radius"), Radius);
		MaterialInstance->SetScalarParameterValue(TEXT("Height"), Height);
		MaterialInstance->SetScalarParameterValue(TEXT("GridGap"), GridGap);
	}
}

void UHexGridPrimitiveComponent::UpdateMaterialParameters()
{
	if (!MaterialInstance)
	{
		return;
	}
	
	// 根据当前渲染模式应用材质参数
	if (RenderMode == EHexGridRenderMode::SingleHexagon)
	{
		ApplyTileTypeConfigsToMaterials();
	}
	else if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		EHexTileType FirstTileType = GetTileType(FIntPoint::ZeroValue);
		ApplyTileTypeConfigsToMaterials();
	}
}

void UHexGridPrimitiveComponent::SetGridSize(int32 NewSizeX, int32 NewSizeY)
{
	if (NewSizeX > 0 && NewSizeY > 0 && (NewSizeX != GridSizeX || NewSizeY != GridSizeY))
	{
		GridSizeX = NewSizeX;
		GridSizeY = NewSizeY;
		
		// 清空地块类型映射，让网格生成时重新生成
		TileTypeMap.Empty();
		
		GenerateHexagonMesh();
		
		// 添加：同步着色器参数
		SyncShaderParameters();
		
		MarkRenderStateDirty();
	}
}

void UHexGridPrimitiveComponent::SetGridGap(float NewGap)
{
	if (NewGap >= 0.0f && NewGap != GridGap)
	{
		GridGap = NewGap;
		GenerateHexagonMesh();
		
		// 添加：同步着色器参数
		SyncShaderParameters();
		
		MarkRenderStateDirty();
	}
}

void UHexGridPrimitiveComponent::SetBaseMaterial(UMaterialInterface* NewMaterial)
{
	if (BaseMaterial != NewMaterial)
	{
		BaseMaterial = NewMaterial;
		CreateMaterialInstance();
		
		// 采用与六棱柱材质相同的简单刷新方案
		MarkRenderStateDirty();
	}
}

void UHexGridPrimitiveComponent::UpdateBounds()
{
	// 计算网格边界框
	FVector GridExtent = FVector(
		(GridSizeX * (Radius * 1.5f + GridGap)) * 0.5f,
		(GridSizeY * (Radius * FMath::Sqrt(3.0f) + GridGap)) * 0.5f,
		Height * 0.5f
	);
	
	FBox LocalBounds = FBox(-GridExtent, GridExtent);
	CachedBounds = FBoxSphereBounds(LocalBounds);
}

// 初始化默认地块类型配置
void UHexGridPrimitiveComponent::InitializeDefaultTileTypeConfigs()
{
	TileTypeConfigs.Empty();
	
	// 草地配置
	FHexTileTypeConfig GrassConfig;
	GrassConfig.TileType = EHexTileType::Grass;
	GrassConfig.BaseColor = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f); // 绿色
	GrassConfig.ColorMultiply = FLinearColor::White;
	GrassConfig.Metallic = 0.0f;
	GrassConfig.Specular = 0.0f;
	GrassConfig.Roughness = 0.8f;
	TileTypeConfigs.Add(GrassConfig);

	// 水域配置
	FHexTileTypeConfig WaterConfig;
	WaterConfig.TileType = EHexTileType::Water;
	WaterConfig.BaseColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.0f); // 蓝色
	WaterConfig.ColorMultiply = FLinearColor::White;
	WaterConfig.Metallic = 0.1f;
	WaterConfig.Specular = 0.9f;
	WaterConfig.Roughness = 0.1f;
	TileTypeConfigs.Add(WaterConfig);

	// 山地配置
	FHexTileTypeConfig MountainConfig;
	MountainConfig.TileType = EHexTileType::Mountain;
	MountainConfig.BaseColor = FLinearColor(0.5f, 0.4f, 0.3f, 1.0f); // 棕色
	MountainConfig.ColorMultiply = FLinearColor::White;
	MountainConfig.Metallic = 0.0f;
	MountainConfig.Specular = 0.0f;
	MountainConfig.Roughness = 0.9f;
	TileTypeConfigs.Add(MountainConfig);

	// 沙漠配置
	FHexTileTypeConfig DesertConfig;
	DesertConfig.TileType = EHexTileType::Desert;
	DesertConfig.BaseColor = FLinearColor(0.9f, 0.8f, 0.4f, 1.0f); // 黄色
	DesertConfig.ColorMultiply = FLinearColor::White;
	DesertConfig.Metallic = 0.0f;
	DesertConfig.Specular = 0.0f;
	DesertConfig.Roughness = 0.7f;
	TileTypeConfigs.Add(DesertConfig);

	// 森林配置
	FHexTileTypeConfig ForestConfig;
	ForestConfig.TileType = EHexTileType::Forest;
	ForestConfig.BaseColor = FLinearColor(0.1f, 0.5f, 0.1f, 1.0f); // 深绿色
	ForestConfig.ColorMultiply = FLinearColor::White;
	ForestConfig.Metallic = 0.0f;
	ForestConfig.Specular = 0.0f;
	ForestConfig.Roughness = 0.8f;
	TileTypeConfigs.Add(ForestConfig);

	// 沼泽配置
	FHexTileTypeConfig SwampConfig;
	SwampConfig.TileType = EHexTileType::Swamp;
	SwampConfig.BaseColor = FLinearColor(0.3f, 0.4f, 0.2f, 1.0f); // 暗绿色
	SwampConfig.ColorMultiply = FLinearColor::White;
	SwampConfig.Metallic = 0.0f;
	SwampConfig.Specular = 0.0f;
	SwampConfig.Roughness = 0.9f;
	TileTypeConfigs.Add(SwampConfig);
}

// 设置单个地块类型
void UHexGridPrimitiveComponent::SetTileType(FIntPoint GridPosition, EHexTileType NewTileType)
{
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		TileTypeMap.Add(GridPosition, NewTileType);
		ApplyTileTypeConfigsToMaterials();
		MarkRenderStateDirty();
	}
}

// 获取地块类型
EHexTileType UHexGridPrimitiveComponent::GetTileType(FIntPoint GridPosition) const
{
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		if (const EHexTileType* FoundType = TileTypeMap.Find(GridPosition))
		{
			return *FoundType;
		}
	}
	return DefaultTileType;
}

// 设置所有地块类型
void UHexGridPrimitiveComponent::SetAllTilesType(EHexTileType TileType)
{
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		TileTypeMap.Empty();
		for (int32 Y = 0; Y < GridSizeY; Y++)
		{
			for (int32 X = 0; X < GridSizeX; X++)
			{
				TileTypeMap.Add(FIntPoint(X, Y), TileType);
			}
		}
		ApplyTileTypeConfigsToMaterials();
		MarkRenderStateDirty();
	}
}

// 更新地块类型配置
void UHexGridPrimitiveComponent::UpdateTileTypeConfig(EHexTileType TileType, const FHexTileTypeConfig& Config)
{
	for (FHexTileTypeConfig& ExistingConfig : TileTypeConfigs)
	{
		if (ExistingConfig.TileType == TileType)
		{
			ExistingConfig = Config;
			break;
		}
	}
	
	// 重新应用所有地块类型的材质参数
	ApplyTileTypeConfigsToMaterials();
	MarkRenderStateDirty();
}

// 获取地块类型配置
FHexTileTypeConfig UHexGridPrimitiveComponent::GetTileTypeConfig(EHexTileType TileType) const
{
	for (const FHexTileTypeConfig& Config : TileTypeConfigs)
	{
		if (Config.TileType == TileType)
		{
			return Config;
		}
	}
	
	// 返回默认配置
	FHexTileTypeConfig DefaultConfig;
	DefaultConfig.TileType = TileType;
	return DefaultConfig;
}

// 获取当前使用的材质
UMaterialInterface* UHexGridPrimitiveComponent::GetCurrentMaterial() const
{
	if (MaterialInstance)
	{
		return MaterialInstance;
	}
	else if (BaseMaterial)
	{
		return BaseMaterial;
	}
	else if (Material)
	{
		return Material;
	}
	return nullptr;
}

// 验证材质实例是否有效
bool UHexGridPrimitiveComponent::IsMaterialInstanceValid() const
{
	return MaterialInstance != nullptr && MaterialInstance->IsValidLowLevel();
}

// 获取材质实例状态信息
FString UHexGridPrimitiveComponent::GetMaterialInstanceStatus() const
{
	if (MaterialInstance)
	{
		return FString::Printf(TEXT("MaterialInstance: Valid, BaseMaterial: %s"), 
			BaseMaterial ? *BaseMaterial->GetName() : TEXT("None"));
	}
	else if (BaseMaterial)
	{
		return FString::Printf(TEXT("MaterialInstance: Failed, BaseMaterial: %s"), *BaseMaterial->GetName());
	}
	else
	{
		return TEXT("MaterialInstance: None, BaseMaterial: None");
	}
}


// 策略A：创建地块类型材质实例
void UHexGridPrimitiveComponent::CreateTileTypeMaterialInstances()
{
	// 如果没有基础材质，使用系统默认材质
	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("BaseMaterial is null, using default material for tile type material instances"));
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	
	// 清空现有材质实例
	for (auto& Pair : TileTypeMaterialInstances)
	{
		if (Pair.Value)
		{
			Pair.Value->RemoveFromRoot();
		}
	}
	TileTypeMaterialInstances.Empty();
	
	// 检查是否使用默认材质
	bool bUsingDefaultMaterial = (BaseMaterial == UMaterial::GetDefaultMaterial(MD_Surface));
	
	// 为每种地块类型创建材质实例
	for (const auto& Config : TileTypeConfigs)
	{
		UMaterialInstanceDynamic* NewMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (NewMaterialInstance)
		{
			// 确保材质实例被正确管理
			NewMaterialInstance->AddToRoot(); // 防止被垃圾回收
			
			// 如果使用默认材质，跳过参数设置，仅创建基础实例
			if (bUsingDefaultMaterial)
			{
				UE_LOG(LogTemp, Log, TEXT("Using default material - skipping parameter setup for tile type: %s"), 
					   *UEnum::GetValueAsString(Config.TileType));
			}
			else
			{
				// 应用地块类型配置到材质实例（仅当不是默认材质时）
				NewMaterialInstance->SetVectorParameterValue(TEXT("color"), Config.BaseColor);
				NewMaterialInstance->SetVectorParameterValue(TEXT("ColorMultiply"), Config.ColorMultiply);
				NewMaterialInstance->SetScalarParameterValue(TEXT("metallic"), Config.Metallic);
				NewMaterialInstance->SetScalarParameterValue(TEXT("specular"), Config.Specular);
				NewMaterialInstance->SetScalarParameterValue(TEXT("roughness"), Config.Roughness);
				
				UE_LOG(LogTemp, Log, TEXT("Applied material parameters for tile type: %s"), 
					   *UEnum::GetValueAsString(Config.TileType));
			}
			
			TileTypeMaterialInstances.Add(Config.TileType, NewMaterialInstance);
			
			UE_LOG(LogTemp, Log, TEXT("Created material instance for tile type: %s"), 
				   *UEnum::GetValueAsString(Config.TileType));
		}
	}
	
	// 确保材质实例在组件中正确注册
	MarkRenderStateDirty();
}

// 策略A：按地块类型分组生成网格
void UHexGridPrimitiveComponent::GenerateTileTypeGroupedMesh()
{
	// 清空现有分组数据
	for (auto& Pair : TileTypeVertices)
	{
		Pair.Value.Empty();
	}
	for (auto& Pair : TileTypeTriangles)
	{
		Pair.Value.Empty();
	}
	for (auto& Pair : TileTypeNormals)
	{
		Pair.Value.Empty();
	}
	for (auto& Pair : TileTypeUVs)
	{
		Pair.Value.Empty();
	}
	for (auto& Pair : TileTypeVertexColors)
	{
		Pair.Value.Empty();
	}
	for (auto& Pair : TileTypeTangents)
	{
		Pair.Value.Empty();
	}
	
	// 计算六边形间距
	float HexWidth = Radius * 2.0f;
	float HexHeight = Radius * FMath::Sqrt(3.0f);
	float HorizontalSpacing = HexWidth * 0.75f + GridGap;
	float VerticalSpacing = HexHeight + GridGap;
	
	// 初始化随机种子
	if (RandomSeed == 0)
	{
		RandomSeed = FMath::RandRange(1000, 9999);
	}
	FMath::RandInit(RandomSeed);
	
	// 清空地块类型映射
	TileTypeMap.Empty();
	
	// 为每种地块类型初始化网格数据
	for (const auto& Config : TileTypeConfigs)
	{
		TileTypeVertices.Add(Config.TileType, TArray<FVector>());
		TileTypeTriangles.Add(Config.TileType, TArray<int32>());
		TileTypeNormals.Add(Config.TileType, TArray<FVector>());
		TileTypeUVs.Add(Config.TileType, TArray<FVector2D>());
		TileTypeVertexColors.Add(Config.TileType, TArray<FLinearColor>());
		TileTypeTangents.Add(Config.TileType, TArray<FProcMeshTangent>());
	}
	
	// 生成每个六棱柱并分配到对应类型组
	for (int32 Y = 0; Y < GridSizeY; Y++)
	{
		for (int32 X = 0; X < GridSizeX; X++)
		{
			FIntPoint GridPos(X, Y);
			EHexTileType RandomTileType = GenerateSimpleRandomTileTypeForPosition(GridPos);
			TileTypeMap.Add(GridPos, RandomTileType);
			
			// 生成单个六棱柱网格数据
			TArray<FVector> HexVertices;
			TArray<int32> HexTriangles;
			TArray<FVector> HexNormals;
			TArray<FVector2D> HexUVs;
			TArray<FLinearColor> HexVertexColors;
			TArray<FProcMeshTangent> HexTangents;
			
			GenerateSingleHexagonMeshData(GridPos, HexVertices, HexTriangles, HexNormals, HexUVs, HexVertexColors, HexTangents);
			
			// 将网格数据添加到对应类型组
			if (TileTypeVertices.Contains(RandomTileType))
			{
				int32 VertexOffset = TileTypeVertices[RandomTileType].Num();
				
				// 添加顶点
				TileTypeVertices[RandomTileType].Append(HexVertices);
				
				// 添加三角形（调整索引）
				for (int32 TriangleIndex : HexTriangles)
				{
					TileTypeTriangles[RandomTileType].Add(TriangleIndex + VertexOffset);
				}
				
				// 添加其他数据
				TileTypeNormals[RandomTileType].Append(HexNormals);
				TileTypeUVs[RandomTileType].Append(HexUVs);
				TileTypeVertexColors[RandomTileType].Append(HexVertexColors);
				TileTypeTangents[RandomTileType].Append(HexTangents);
			}
			
			// UE_LOG(LogTemp, Log, TEXT("Generated tile at (%d, %d) with type: %s"), 
			// 	   X, Y, *UEnum::GetValueAsString(RandomTileType));
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("Generated grouped mesh for %d tile types"), TileTypeConfigs.Num());
	
	// 添加详细的网格数据日志
	for (const auto& Pair : TileTypeVertices)
	{
		EHexTileType TileType = Pair.Key;
		const TArray<FVector>& TileVertices = Pair.Value;
		const TArray<int32>& TileTriangles = TileTypeTriangles[TileType];
		
		UE_LOG(LogTemp, Warning, TEXT("TileType %s: %d vertices, %d triangles"), 
			   *UEnum::GetValueAsString(TileType), TileVertices.Num(), TileTriangles.Num());
		
		// 打印前几个顶点的位置（调试用）
		if (TileVertices.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  First vertex: %s"), *TileVertices[0].ToString());
			if (TileVertices.Num() > 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("  Second vertex: %s"), *TileVertices[1].ToString());
			}
			if (TileVertices.Num() > 7)
			{
				UE_LOG(LogTemp, Warning, TEXT("  Top center vertex: %s"), *TileVertices[7].ToString());
			}
		}
		
		// 打印前几个三角形索引（调试用）
		if (TileTriangles.Num() >= 3)
		{
			UE_LOG(LogTemp, Warning, TEXT("  First triangle: %d, %d, %d"), 
				   TileTriangles[0], TileTriangles[1], TileTriangles[2]);
		}
		if (TileTriangles.Num() >= 6)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Second triangle: %d, %d, %d"), 
				   TileTriangles[3], TileTriangles[4], TileTriangles[5]);
		}
	}
}

// 生成单个六棱柱网格数据（用于分组）
void UHexGridPrimitiveComponent::GenerateSingleHexagonMeshData(
	FIntPoint GridPos, 
	TArray<FVector>& OutVertices, 
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FVector2D>& OutUVs,
	TArray<FLinearColor>& OutVertexColors,
	TArray<FProcMeshTangent>& OutTangents)
{
	FVector WorldOffset = CalculateHexagonWorldPosition(GridPos);
	
	// 底面中心点 (索引0)
	OutVertices.Add(WorldOffset + FVector(0, 0, 0));
	OutUVs.Add(FVector2D(0.5f, 0.5f));
	OutNormals.Add(FVector(0, 0, -1));
	OutVertexColors.Add(FLinearColor::White);
	OutTangents.Add(FProcMeshTangent(1, 0, 0));
	
	// 底面6个顶点 (索引1-6)
	for (int32 i = 0; i < 6; i++)
	{
		FVector LocalVertex = CalculateHexagonVertex(i, Radius, 0.0f);
		OutVertices.Add(WorldOffset + LocalVertex);
		OutUVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(FMath::DegreesToRadians(60.0f * i)), 
							  0.5f + 0.5f * FMath::Sin(FMath::DegreesToRadians(60.0f * i))));
		OutNormals.Add(FVector(0, 0, -1));
		OutVertexColors.Add(FLinearColor::White);
		OutTangents.Add(FProcMeshTangent(1, 0, 0));
	}
	
	// 顶面中心点 (索引7)
	OutVertices.Add(WorldOffset + FVector(0, 0, Height));
	OutUVs.Add(FVector2D(0.5f, 0.5f));
	OutNormals.Add(FVector(0, 0, 1));
	OutVertexColors.Add(FLinearColor::White);
	OutTangents.Add(FProcMeshTangent(1, 0, 0));
	
	// 顶面6个顶点 (索引8-13)
	for (int32 i = 0; i < 6; i++)
	{
		FVector LocalVertex = CalculateHexagonVertex(i, Radius, Height);
		OutVertices.Add(WorldOffset + LocalVertex);
		OutUVs.Add(FVector2D(0.5f + 0.5f * FMath::Cos(FMath::DegreesToRadians(60.0f * i)), 
							  0.5f + 0.5f * FMath::Sin(FMath::DegreesToRadians(60.0f * i))));
		OutNormals.Add(FVector(0, 0, 1));
		OutVertexColors.Add(FLinearColor::White);
		OutTangents.Add(FProcMeshTangent(1, 0, 0));
	}
	
	// 生成三角形
	// 底面三角形（6个扇形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		OutTriangles.Add(0); // 底面中心点
		OutTriangles.Add(i + 1);
		OutTriangles.Add(Next + 1);
	}
	
	// 顶面三角形（6个扇形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		OutTriangles.Add(7); // 顶面中心点
		OutTriangles.Add(7 + Next + 1); // 下一个顶点
		OutTriangles.Add(7 + i + 1);     // 当前顶点
	}
	
	// 侧面（6个四边形，每个2个三角形）
	for (int32 i = 0; i < 6; i++)
	{
		int32 Next = (i + 1) % 6;
		int32 BottomCurrent = i + 1;
		int32 BottomNext = Next + 1;
		int32 TopCurrent = 7 + i + 1;
		int32 TopNext = 7 + Next + 1;
		
		// 计算侧面法线
		FVector SideNormal = FVector::CrossProduct(
			OutVertices[TopCurrent] - OutVertices[BottomCurrent],
			OutVertices[BottomNext] - OutVertices[BottomCurrent]
		).GetSafeNormal();
		
		// 更新侧面顶点法线
		OutNormals[BottomCurrent] = (OutNormals[BottomCurrent] + SideNormal).GetSafeNormal();
		OutNormals[BottomNext] = (OutNormals[BottomNext] + SideNormal).GetSafeNormal();
		OutNormals[TopCurrent] = (OutNormals[TopCurrent] + SideNormal).GetSafeNormal();
		OutNormals[TopNext] = (OutNormals[TopNext] + SideNormal).GetSafeNormal();
		
		// 三角形1
		OutTriangles.Add(BottomCurrent);
		OutTriangles.Add(TopNext);
		OutTriangles.Add(BottomNext);
		
		// 三角形2
		OutTriangles.Add(BottomCurrent);
		OutTriangles.Add(TopCurrent);
		OutTriangles.Add(TopNext);
	}
}

// 计算六棱柱法线
FVector UHexGridPrimitiveComponent::CalculateHexagonNormal(int32 VertexIndex) const
{
	// 根据顶点索引计算法线
	if (VertexIndex < 6)
	{
		// 底面顶点，法线向下
		return FVector(0.0f, 0.0f, -1.0f);
	}
	else if (VertexIndex < 12)
	{
		// 顶面顶点，法线向上
		return FVector(0.0f, 0.0f, 1.0f);
	}
	else
	{
		// 侧面顶点，法线向外
		int32 SideIndex = VertexIndex - 12;
		float Angle = SideIndex * UE_PI / 3.0f;
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}
}

// 计算六棱柱UV坐标
FVector2D UHexGridPrimitiveComponent::CalculateHexagonUV(int32 VertexIndex) const
{
	// 根据顶点索引计算UV坐标
	if (VertexIndex < 6)
	{
		// 底面顶点
		float Angle = VertexIndex * UE_PI / 3.0f;
		float U = 0.5f + 0.5f * FMath::Cos(Angle);
		float V = 0.5f + 0.5f * FMath::Sin(Angle);
		return FVector2D(U, V);
	}
	else if (VertexIndex < 12)
	{
		// 顶面顶点
		float Angle = (VertexIndex - 6) * UE_PI / 3.0f;
		float U = 0.5f + 0.5f * FMath::Cos(Angle);
		float V = 0.5f + 0.5f * FMath::Sin(Angle);
		return FVector2D(U, V);
	}
	else
	{
		// 侧面顶点
		int32 SideIndex = VertexIndex - 12;
		float U = (SideIndex % 2) == 0 ? 0.0f : 1.0f;
		float V = (SideIndex / 2) / 3.0f;
		return FVector2D(U, V);
	}
}

// 计算六棱柱切线
FProcMeshTangent UHexGridPrimitiveComponent::CalculateHexagonTangent(int32 VertexIndex) const
{
	// 根据顶点索引计算切线
	if (VertexIndex < 6)
	{
		// 底面顶点
		float Angle = VertexIndex * UE_PI / 3.0f;
		return FProcMeshTangent(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}
	else if (VertexIndex < 12)
	{
		// 顶面顶点
		float Angle = (VertexIndex - 6) * UE_PI / 3.0f;
		return FProcMeshTangent(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}
	else
	{
		// 侧面顶点
		int32 SideIndex = VertexIndex - 12;
		float Angle = SideIndex * UE_PI / 3.0f;
		return FProcMeshTangent(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f);
	}
}

// 应用所有地块类型配置到材质实例
void UHexGridPrimitiveComponent::ApplyTileTypeConfigsToMaterials()
{
	for (const auto& Config : TileTypeConfigs)
	{
		if (TileTypeMaterialInstances.Contains(Config.TileType))
		{
			UMaterialInstanceDynamic* TileMaterialInstance = TileTypeMaterialInstances[Config.TileType];
			if (TileMaterialInstance)
			{
				// 应用地块类型配置到材质实例
				TileMaterialInstance->SetVectorParameterValue(TEXT("color"), Config.BaseColor);
				TileMaterialInstance->SetVectorParameterValue(TEXT("ColorMultiply"), Config.ColorMultiply);
				TileMaterialInstance->SetScalarParameterValue(TEXT("metallic"), Config.Metallic);
				TileMaterialInstance->SetScalarParameterValue(TEXT("specular"), Config.Specular);
				TileMaterialInstance->SetScalarParameterValue(TEXT("roughness"), Config.Roughness);
				
				UE_LOG(LogTemp, Log, TEXT("Applied material parameters for tile type: %s"), 
					   *UEnum::GetValueAsString(Config.TileType));
			}
		}
	}
}

// 为指定位置生成随机地块类型
EHexTileType UHexGridPrimitiveComponent::GenerateRandomTileTypeForPosition(FIntPoint GridPosition)
{
	// 如果可用类型为空，返回默认类型
	if (AvailableTileTypes.Num() == 0)
	{
		return DefaultTileType;
	}
	
	// 使用位置作为随机种子的一部分，确保相同位置总是生成相同类型
	int32 PositionSeed = GridPosition.X * 1000 + GridPosition.Y + RandomSeed;
	FMath::RandInit(PositionSeed);
	
	// 随机选择地块类型
	int32 RandomIndex = FMath::RandRange(0, AvailableTileTypes.Num() - 1);
	return AvailableTileTypes[RandomIndex];
}

// 简化版的随机地块类型生成，直接从grass和water中随机选择
EHexTileType UHexGridPrimitiveComponent::GenerateSimpleRandomTileTypeForPosition(FIntPoint GridPosition)
{
	// 直接从grass和water中随机选择
	return FMath::RandRange(0, 1) == 0 ? EHexTileType::Grass : EHexTileType::Water;
}

// 验证所有材质实例是否有效
bool UHexGridPrimitiveComponent::ValidateTileTypeMaterialInstances() const
{
	if (RenderMode != EHexGridRenderMode::HexGrid)
	{
		return true; // 非网格模式不需要验证
	}
	
	bool bAllValid = true;
	for (const auto& Pair : TileTypeMaterialInstances)
	{
		if (!Pair.Value || !Pair.Value->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid material instance for tile type: %s"), 
				   *UEnum::GetValueAsString(Pair.Key));
			bAllValid = false;
		}
	}
	
	return bAllValid;
}

// 新增：材质设置处理方法
void UHexGridPrimitiveComponent::HandleMaterialSetup()
{
	// 确保在非选中状态下也创建材质
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		if (!BaseMaterial)
		{
			// 使用有效的默认材质
			BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
		}
		CreateTileTypeMaterialInstances();
	}
	else
	{
		CreateMaterialInstance();
	}
}

// 新增：网格模式材质处理
void UHexGridPrimitiveComponent::HandleGridModeMaterial()
{
	// 确保有基础材质
	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: No BaseMaterial for grid mode, using default"));
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	
	// 验证基础材质有效性
	if (!BaseMaterial->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: BaseMaterial is invalid"));
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	
	// 创建地块类型材质实例
	CreateTileTypeMaterialInstances();
	
	// 验证材质实例创建结果
	if (!ValidateTileTypeMaterialInstances())
	{
		UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: Failed to create tile type material instances"));
	}
}

// 新增：单个模式材质处理
void UHexGridPrimitiveComponent::HandleSingleModeMaterial()
{
	// 优先使用基础材质创建材质实例
	if (BaseMaterial && BaseMaterial->IsValidLowLevel())
	{
		CreateMaterialInstance();
		if (MaterialInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Material instance created successfully"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: Failed to create material instance"));
		}
	}
	// 回退到传统材质
	else if (Material && Material->IsValidLowLevel())
	{
		SetMaterial(0, Material);
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Using traditional material"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: No valid material found, using default"));
		// 设置默认材质
		SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
	}
}

// 新增：组件状态验证
void UHexGridPrimitiveComponent::ValidateComponentState()
{
	// 验证网格数据
	if (Vertices.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: No vertices generated"));
		return;
	}
	
	// 验证材质状态
	UMaterialInterface* CurrentMaterial = GetMaterial(0);
	if (!CurrentMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: No material set"));
		return;
	}
	
	// 验证地块类型材质实例（仅网格模式）
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		if (TileTypeMaterialInstances.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("UHexGridPrimitiveComponent: No tile type material instances created"));
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Component state validated successfully"));
}


// 切换包围盒显示
void UHexGridPrimitiveComponent::ToggleDebugBounds()
{
	bShowDebugBounds = !bShowDebugBounds;
	UE_LOG(LogTemp, Warning, TEXT("Debug bounds display: %s"), bShowDebugBounds ? TEXT("ON") : TEXT("OFF"));
	MarkRenderStateDirty();
}

// 切换网格信息显示
void UHexGridPrimitiveComponent::ToggleDebugGrid()
{
	bShowDebugGrid = !bShowDebugGrid;
	UE_LOG(LogTemp, Warning, TEXT("Debug grid display: %s"), bShowDebugGrid ? TEXT("ON") : TEXT("OFF"));
	MarkRenderStateDirty();
}

// 启用调试绘制
void UHexGridPrimitiveComponent::EnableDebugDrawing()
{
	bShowDebugBounds = true;
	bShowDebugGrid = true;
	UE_LOG(LogTemp, Warning, TEXT("Debug drawing enabled"));
	MarkRenderStateDirty();
}

// 禁用调试绘制
void UHexGridPrimitiveComponent::DisableDebugDrawing()
{
	bShowDebugBounds = false;
	bShowDebugGrid = false;
	UE_LOG(LogTemp, Warning, TEXT("Debug drawing disabled"));
	MarkRenderStateDirty();
}


