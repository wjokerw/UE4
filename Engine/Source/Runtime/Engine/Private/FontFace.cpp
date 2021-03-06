// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/FontFace.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "EditorObjectVersion.h"

UFontFace::UFontFace()
	: FontFaceData(FFontFaceData::MakeFontFaceData())
{
}

void UFontFace::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	const FString LocalSourceFilename = SourceFilename;

	if (Ar.IsCooking())
	{
		// Set the cooked filename prior to serializing the properties
		SourceFilename = GetCookedFilename();
	}

	Super::Serialize(Ar);

	if (Ar.IsCooking())
	{
		// Restore the original filename after serializing the properties
		SourceFilename = LocalSourceFilename;
	}

	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::AddedInlineFontFaceAssets)
		{
#if WITH_EDITORONLY_DATA
			// Port the old property data into the shared instance
			FontFaceData->SetData(MoveTemp(FontFaceData_DEPRECATED));
#endif // WITH_EDITORONLY_DATA
		}
		else
		{
			bool bLoadInlineData = false;
			Ar << bLoadInlineData;

			if (bLoadInlineData)
			{
				if (FontFaceData->HasData())
				{
					// If we already have data, make a new instance in-case the existing one is being referenced by the font cache
					FontFaceData = FFontFaceData::MakeFontFaceData();
				}
				FontFaceData->Serialize(Ar);
			}
		}
	}
	else
	{
		// Only save the inline data in a cooked build if we're using the inline loading policy
		bool bSaveInlineData = LoadingPolicy == EFontLoadingPolicy::Inline || !Ar.IsCooking();
		Ar << bSaveInlineData;

		if (bSaveInlineData)
		{
			FontFaceData->Serialize(Ar);
		}
	}
}

void UFontFace::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// Only count the memory size for fonts that will be loaded
	if (LoadingPolicy != EFontLoadingPolicy::Stream)
	{
		const bool bCountInlineData = WITH_EDITORONLY_DATA || LoadingPolicy == EFontLoadingPolicy::Inline;
		if (bCountInlineData)
		{
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FontFaceData->GetData().Num());
		}
		else
		{
			const int64 FileSize = IFileManager::Get().FileSize(*SourceFilename);
			if (FileSize > 0)
			{
				CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FileSize);
			}
		}
	}
}

#if WITH_EDITOR
void UFontFace::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FSlateApplication::Get().GetRenderer()->FlushFontCache();
}

void UFontFace::PostEditUndo()
{
	Super::PostEditUndo();

	FSlateApplication::Get().GetRenderer()->FlushFontCache();
}

void UFontFace::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FAssetImportInfo ImportInfo;
	ImportInfo.Insert(FAssetImportInfo::FSourceFile(SourceFilename));
	OutTags.Add(FAssetRegistryTag(SourceFileTagName(), ImportInfo.ToJson(), FAssetRegistryTag::TT_Hidden));
}

void UFontFace::CookAdditionalFiles(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform)
{
	Super::CookAdditionalFiles(PackageFilename, TargetPlatform);

	if (LoadingPolicy != EFontLoadingPolicy::Inline)
	{
		// We replace the package name with the cooked font face name
		// Note: This must match the replacement logic in UFontFace::GetCookedFilename
		const FString CookedFontFilename = FPaths::GetPath(PackageFilename) / GetName() + TEXT(".ufont");
		FFileHelper::SaveArrayToFile(FontFaceData->GetData(), *CookedFontFilename);
	}
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UFontFace::InitializeFromBulkData(const FString& InFilename, const EFontHinting InHinting, const void* InBulkDataPtr, const int32 InBulkDataSizeBytes)
{
	check(InBulkDataPtr && InBulkDataSizeBytes > 0 && !FontFaceData->HasData());

	SourceFilename = InFilename;
	Hinting = InHinting;
	LoadingPolicy = EFontLoadingPolicy::LazyLoad;
	
	TArray<uint8> FontData;
	FontData.Append(static_cast<const uint8*>(InBulkDataPtr), InBulkDataSizeBytes);
	FontFaceData->SetData(MoveTemp(FontData));
}
#endif // WITH_EDITORONLY_DATA

const FString& UFontFace::GetFontFilename() const
{
	return SourceFilename;
}

EFontHinting UFontFace::GetHinting() const
{
	return Hinting;
}

EFontLoadingPolicy UFontFace::GetLoadingPolicy() const
{
	return LoadingPolicy;
}

FFontFaceDataConstRef UFontFace::GetFontFaceData() const
{
	return FontFaceData;
}

FString UFontFace::GetCookedFilename() const
{
	// UFontFace assets themselves can't be localized, however that doesn't mean the package they're in isn't localized (ie, when they're upgraded into a UFont asset)
	FString PackageName = GetOutermost()->GetName();
	PackageName = FPackageName::GetLocalizedPackagePath(PackageName);
	
	// Note: This must match the replacement logic in UFontFace::CookAdditionalFiles
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
	return FPaths::GetPath(PackageFilename) / GetName() + TEXT(".ufont");
}
