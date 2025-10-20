// Fill out your copyright notice in the Description page of Project Settings.

#include "HexGridPrimitiveComponent.h"
#include "HexGridPrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
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
}

void UHexGridPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
	
	// 组件注册时生成网格
	GenerateHexagonMesh();
}

void UHexGridPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// 优先创建材质实例
	if (BaseMaterial)
	{
		CreateMaterialInstance();
		// 确保材质正确应用
		if (MaterialInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Material instance created successfully in BeginPlay"));
		}
	}
	// 如果没有基础材质，使用传统材质
	else if (Material)
	{
		SetMaterial(0, Material);
		UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Using traditional material in BeginPlay"));
	}
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
		// 重新应用当前地块类型的材质参数
		if (RenderMode == EHexGridRenderMode::SingleHexagon)
		{
			ApplyTileTypeToMaterial(DefaultTileType);
		}
		else if (RenderMode == EHexGridRenderMode::HexGrid)
		{
			EHexTileType FirstTileType = GetTileType(FIntPoint::ZeroValue);
			ApplyTileTypeToMaterial(FirstTileType);
		}
		MarkRenderStateDirty();
	}
	// 检查默认地块类型变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexGridPrimitiveComponent, DefaultTileType))
	{
		if (RenderMode == EHexGridRenderMode::SingleHexagon)
		{
			ApplyTileTypeToMaterial(DefaultTileType);
			MarkRenderStateDirty();
		}
	}
	// 检查地块配置中的颜色参数变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, BaseColor) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, ColorMultiply) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, Metallic) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, Specular) ||
	         PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FHexTileTypeConfig, Roughness))
	{
		// 重新应用当前地块类型的材质参数
		if (RenderMode == EHexGridRenderMode::SingleHexagon)
		{
			ApplyTileTypeToMaterial(DefaultTileType);
		}
		else if (RenderMode == EHexGridRenderMode::HexGrid)
		{
			EHexTileType FirstTileType = GetTileType(FIntPoint::ZeroValue);
			ApplyTileTypeToMaterial(FirstTileType);
		}
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

FBoxSphereBounds UHexGridPrimitiveComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		return CachedBounds.TransformBy(LocalToWorld);
	}
	else
	{
		// 单个六棱柱的边界框
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
	
	// 应用第一个地块的材质（当前系统限制）
	if (TileTypeMap.Num() > 0)
	{
		EHexTileType FirstTileType = GetTileType(FIntPoint::ZeroValue);
		ApplyTileTypeToMaterial(FirstTileType);
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
		ApplyTileTypeToMaterial(DefaultTileType);
	}
	else if (RenderMode == EHexGridRenderMode::HexGrid)
	{
		EHexTileType FirstTileType = GetTileType(FIntPoint::ZeroValue);
		ApplyTileTypeToMaterial(FirstTileType);
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
		ApplyTileTypeToMaterial(NewTileType);
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
		ApplyTileTypeToMaterial(TileType);
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
	
	// 如果当前显示的是这个类型，立即应用
	if (RenderMode == EHexGridRenderMode::SingleHexagon || 
		(RenderMode == EHexGridRenderMode::HexGrid && GetTileType(FIntPoint::ZeroValue) == TileType))
	{
		ApplyTileTypeToMaterial(TileType);
		MarkRenderStateDirty();
	}
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

// 应用地块类型到材质
void UHexGridPrimitiveComponent::ApplyTileTypeToMaterial(EHexTileType TileType)
{
	// 打印--------------
	UE_LOG(LogTemp, Warning, TEXT("-----------------"));
	// 打印--------------

	if (!MaterialInstance)
	{
		return;
	}

	FHexTileTypeConfig Config = GetTileTypeConfig(TileType);

	// 应用地块类型时打印日志
	UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Applying tile type %s to material"), 
		*UEnum::GetValueAsString(TileType));
	
	// 应用颜色参数
	MaterialInstance->SetVectorParameterValue(TEXT("color"), Config.BaseColor);
	MaterialInstance->SetVectorParameterValue(TEXT("ColorMultiply"), Config.ColorMultiply);
	
	// 应用材质属性参数
	MaterialInstance->SetScalarParameterValue(TEXT("metallic"), Config.Metallic);
	MaterialInstance->SetScalarParameterValue(TEXT("specular"), Config.Specular);
	MaterialInstance->SetScalarParameterValue(TEXT("roughness"), Config.Roughness);

	// 实时打印最新的材质参数内容
	UE_LOG(LogTemp, Warning, TEXT("UHexGridPrimitiveComponent: Updated material parameters:"));
	UE_LOG(LogTemp, Warning, TEXT("  - BaseColor: R=%.3f, G=%.3f, B=%.3f, A=%.3f"), 
		Config.BaseColor.R, Config.BaseColor.G, Config.BaseColor.B, Config.BaseColor.A);
	UE_LOG(LogTemp, Warning, TEXT("  - ColorMultiply: R=%.3f, G=%.3f, B=%.3f, A=%.3f"), 
		Config.ColorMultiply.R, Config.ColorMultiply.G, Config.ColorMultiply.B, Config.ColorMultiply.A);
	UE_LOG(LogTemp, Warning, TEXT("  - Metallic: %.3f"), Config.Metallic);
	UE_LOG(LogTemp, Warning, TEXT("  - Specular: %.3f"), Config.Specular);
	UE_LOG(LogTemp, Warning, TEXT("  - Roughness: %.3f"), Config.Roughness);

	// 关键修复：通知渲染系统刷新
	MarkRenderStateDirty();

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

// 检查是否需要重新生成地块类型
bool UHexGridPrimitiveComponent::ShouldRegenerateTileTypes() const
{
	// 如果地块类型映射为空，或者网格尺寸发生了变化，需要重新生成
	return TileTypeMap.Num() == 0 || 
		   TileTypeMap.Num() != (GridSizeX * GridSizeY);
}

// 设置随机种子
void UHexGridPrimitiveComponent::SetRandomSeed(int32 NewSeed)
{
	RandomSeed = NewSeed;
	UE_LOG(LogTemp, Log, TEXT("Set random seed to: %d"), RandomSeed);
}

// 设置可用地块类型
void UHexGridPrimitiveComponent::SetAvailableTileTypes(const TArray<EHexTileType>& NewTypes)
{
	AvailableTileTypes = NewTypes;
	
	// 如果类型数组为空，设置默认值
	if (AvailableTileTypes.Num() == 0)
	{
		AvailableTileTypes.Add(EHexTileType::Grass);
		AvailableTileTypes.Add(EHexTileType::Water);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Set available tile types: %d types"), AvailableTileTypes.Num());
}

// 智能重新生成（只在需要时重新生成）
void UHexGridPrimitiveComponent::SmartRegenerateTiles()
{
	if (ShouldRegenerateTileTypes())
	{
		UE_LOG(LogTemp, Log, TEXT("SmartRegenerateTiles: Regenerating tiles due to empty or mismatched tile map"));
		GenerateHexGridMesh();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("SmartRegenerateTiles: No regeneration needed"));
	}
}

// 强制重新生成所有地块类型
void UHexGridPrimitiveComponent::ForceRegenerateAllTiles()
{
	// 清空现有映射
	TileTypeMap.Empty();
	
	// 重新生成网格
	GenerateHexagonMesh();
	
	UE_LOG(LogTemp, Warning, TEXT("ForceRegenerateAllTiles: Forced regeneration of all tiles"));
}

// 重新生成随机地块（蓝图友好）
void UHexGridPrimitiveComponent::RegenerateRandomTiles()
{
	GenerateRandomTileTypes();
}

// 设置草地和水类型
void UHexGridPrimitiveComponent::SetGrassAndWaterTypes()
{
	AvailableTileTypes.Empty();
	AvailableTileTypes.Add(EHexTileType::Grass);
	AvailableTileTypes.Add(EHexTileType::Water);
	
	UE_LOG(LogTemp, Log, TEXT("Set available types to Grass and Water"));
}

// 获取地块类型统计
TMap<EHexTileType, int32> UHexGridPrimitiveComponent::GetTileTypeStatistics() const
{
	TMap<EHexTileType, int32> Statistics;
	
	for (const auto& Pair : TileTypeMap)
	{
		EHexTileType TileType = Pair.Value;
		if (int32* Count = Statistics.Find(TileType))
		{
			(*Count)++;
		}
		else
		{
			Statistics.Add(TileType, 1);
		}
	}
	
	return Statistics;
}

// 生成随机地块类型
void UHexGridPrimitiveComponent::GenerateRandomTileTypes()
{
	if (RenderMode != EHexGridRenderMode::HexGrid)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateRandomTileTypes: Only available in HexGrid mode"));
		return;
	}
	
	// 设置随机种子
	if (RandomSeed == 0)
	{
		RandomSeed = FMath::RandRange(1000, 9999);
	}
	
	// 设置默认可用类型
	if (AvailableTileTypes.Num() == 0)
	{
		AvailableTileTypes.Add(EHexTileType::Grass);
		AvailableTileTypes.Add(EHexTileType::Water);
	}
	
	// 清空现有映射
	TileTypeMap.Empty();
	
	// 重新生成网格（这会自动生成地块类型）
	GenerateHexagonMesh();
	
	UE_LOG(LogTemp, Warning, TEXT("Regenerated grid with random tile types"));
}


