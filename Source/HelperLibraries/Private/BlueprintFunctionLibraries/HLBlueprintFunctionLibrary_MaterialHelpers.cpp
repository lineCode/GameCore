// Fill out your copyright notice in the Description page of Project Settings.


#include "BlueprintFunctionLibraries/HLBlueprintFunctionLibrary_MaterialHelpers.h"



int32 UHLBlueprintFunctionLibrary_MaterialHelpers::GetMaterialIndexFromSectionIndex(const UStaticMeshComponent* StaticMeshComponent, const int32 SectionIndex)
{
    // Adapted from GetMaterialFromCollisionFaceIndex()
    const UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (Mesh && RenderData && SectionIndex >= 0)
    {
        const int32 LODIndex = Mesh->LODForCollision;
        if (RenderData->LODResources.IsValidIndex(LODIndex))
        {
            const FStaticMeshLODResources& LODResource = RenderData->LODResources[LODIndex];
            if (SectionIndex < LODResource.Sections.Num())
            {
                return LODResource.Sections[SectionIndex].MaterialIndex;
            }
        }
    }

    return -1;
}
