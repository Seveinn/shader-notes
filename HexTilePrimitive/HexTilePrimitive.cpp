// Fill out your copyright notice in the Description page of Project Settings.

#include "HexTilePrimitive.h"
#include "HexTilePrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"

UHexTilePrimitive::UHexTilePrimitive(const FObjectInitializer& ObjectInitializer)
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
}

void UHexTilePrimitive::OnRegister()
{
	Super::OnRegister();
	
	// 组件注册时生成网格
	GenerateHexagonMesh();
}

void UHexTilePrimitive::BeginPlay()
{
	Super::BeginPlay();
	
	// 应用材质
	if (Material)
	{
		SetMaterial(0, Material);
	}
}

void UHexTilePrimitive::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	// 检查是否是材质属性变更
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexTilePrimitive, Material))
	{
		// 立即应用新材质
		if (Material)
		{
			SetMaterial(0, Material);
		}
		else
		{
			// 如果材质为空，清除材质
			SetMaterial(0, nullptr);
		}
		MarkRenderStateDirty();
	}
	// 检查其他需要重新生成网格的属性
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexTilePrimitive, Radius) ||
			 PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexTilePrimitive, Height))
	{
		// 重新生成网格
		GenerateHexagonMesh();
		MarkRenderStateDirty();
	}
	// 检查线框属性变更
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHexTilePrimitive, bShowWireframe))
	{
		MarkRenderStateDirty();
	}
}

void UHexTilePrimitive::SetRadius(float NewRadius)
{
	if (NewRadius > 0.0f && NewRadius != Radius)
	{
		Radius = NewRadius;
		RegenerateHexagon();
	}
}

void UHexTilePrimitive::SetHeight(float NewHeight)
{
	if (NewHeight > 0.0f && NewHeight != Height)
	{
		Height = NewHeight;
		RegenerateHexagon();
	}
}

void UHexTilePrimitive::SetHexMaterial(UMaterialInterface* NewMaterial)
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

void UHexTilePrimitive::RegenerateHexagon()
{
	GenerateHexagonMesh();
	MarkRenderStateDirty();
}

TArray<FVector> UHexTilePrimitive::GetHexagonVerticesWorld() const
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

void UHexTilePrimitive::ToggleWireframe()
{
	bShowWireframe = !bShowWireframe;
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UHexTilePrimitive::CreateSceneProxy()
{
	return new FHexTilePrimitiveSceneProxy(this);
}

UMaterialInterface* UHexTilePrimitive::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0 && Material)
	{
		return Material;
	}
	return nullptr;
}

int32 UHexTilePrimitive::GetNumMaterials() const
{
	return 1;
}

void UHexTilePrimitive::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	OutMaterials.Empty();
	if (Material)
	{
		OutMaterials.Add(Material);
	}
}

FBoxSphereBounds UHexTilePrimitive::CalcBounds(const FTransform& LocalToWorld) const
{
	// 计算六棱柱的边界框
	FVector Extent = FVector(Radius, Radius, Height * 0.5f);
	FBox LocalBounds = FBox(-Extent, Extent);
	return FBoxSphereBounds(LocalBounds.TransformBy(LocalToWorld));
}

FVector UHexTilePrimitive::CalculateHexagonVertex(int32 VertexIndex, float InRadius, float Z) const
{
	// 六边形顶点角度：0度为右侧，顺时针旋转
	// 角度序列：0°, 60°, 120°, 180°, 240°, 300°
	float AngleDeg = 60.0f * VertexIndex;
	float AngleRad = FMath::DegreesToRadians(AngleDeg);
	
	float X = InRadius * FMath::Cos(AngleRad);
	float Y = InRadius * FMath::Sin(AngleRad);
	
	return FVector(X, Y, Z);
}

void UHexTilePrimitive::GenerateHexagonMesh()
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
		UE_LOG(LogTemp, Warning, TEXT("UHexTilePrimitive: Invalid dimensions - Radius: %f, Height: %f"), Radius, Height);
		return;
	}
	
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
	
	// 更新边界框缓存
	CachedBounds = CalcBounds(FTransform::Identity);
	
	UE_LOG(LogTemp, Warning, TEXT("UHexTilePrimitive: Generated %d vertices, %d triangles"), 
		   Vertices.Num(), Triangles.Num());
}

