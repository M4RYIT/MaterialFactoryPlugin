// Fill out your copyright notice in the Description page of Project Settings.

#include "MatFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/TextureDefines.h"
#include "Factories/MaterialFactoryNew.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

UMatFactory::UMatFactory()
{
	bEditorImport = true;
	SupportedClass = UMaterial::StaticClass();
	Formats.Add(TEXT("mt;Mt file format"));
	InitNodesFunction();
}

UObject* UMatFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	Nodes.Empty();
	MaterialInputs.Empty();

	UMaterial* Material = CreateMaterial(InParent, InName, Flags, Filename);
	bOutOperationCanceled = !Material;
	return Material;
}

UMaterial* UMatFactory::CreateMaterial(UObject* Package, FName MaterialName, EObjectFlags Flags, const FString& Filepath)
{
	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *Filepath);

	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UMaterial* Material = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(MaterialFactory->SupportedClass, Package, 
										  MaterialName, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, nullptr, GWarn));
	InitMaterialInputs(Material);

	bool bResult = true;
	for (const FString& Line : Lines)
	{
		TArray<FString> Actions;
		Line.ParseIntoArrayWS(Actions);

		if (Actions.Num() > 0)
		{
			if (Actions[0].StartsWith(CONNECT_CMD))
			{
				bResult &= LinkNodes(Actions);
			}
			else
			{
				bResult &= AddNode(Actions, Material);
			}
		}
	}

	return (bResult) ? Material: nullptr;
}

UTexture2D* UMatFactory::LoadTexture(UObject* MaterialPackage, const FString& FilePath)
{
	TArray<uint8> Buffer;
	FFileHelper::LoadFileToArray(Buffer, *FilePath);

	TArray<FString> PathTokens;
	TArray<const TCHAR*> Delimiters = { TEXT("\\"), TEXT("."), TEXT("/")};
	FilePath.ParseIntoArray(PathTokens, Delimiters.GetData(), Delimiters.Num(), true);

	TArray<FString> PackageTokens;
	MaterialPackage->GetPackage()->GetName().ParseIntoArray(PackageTokens, Delimiters[2]);
	PackageTokens.Pop(true);
	FString PackageName = FString::Join(PackageTokens, Delimiters[2]);
	UPackage* Package = CreatePackage(*FString::Printf(TEXT("/%s/%s"), *PackageName, *PathTokens[PathTokens.Num() - 2]));
	FCreateTexture2DParameters TexParams;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	EImageFormat Format = ImageWrapperModule.DetectImageFormat(Buffer.GetData(), Buffer.GetAllocatedSize());

	if (Format != EImageFormat::Invalid)
	{
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(Format);
		if (ImageWrapper->SetCompressed((void*)Buffer.GetData(), Buffer.GetAllocatedSize()))
		{
			int32 BitDepth = ImageWrapper->GetBitDepth();
			int32 Width = ImageWrapper->GetWidth();
			int32 Height = ImageWrapper->GetHeight();

			if (BitDepth == 8 || BitDepth == 16)
			{
				TArray64<uint8> UncompressedData;
				ImageWrapper->GetRaw(ERGBFormat::BGRA, BitDepth, UncompressedData);

				UTexture2D* Tex = NewObject<UTexture2D>(Package, *PathTokens[PathTokens.Num() - 2], EObjectFlags::RF_Public | EObjectFlags::RF_Standalone);
				FAssetRegistryModule::AssetCreated(Tex);
				Tex->Source.Init(Width, Height, 1, 1, TSF_BGRA8);
				uint8* TexData = Tex->Source.LockMip(0);
				FMemory::Memcpy(TexData, UncompressedData.GetData(), SIZE_T(Width * Height * 4));
				Tex->Source.UnlockMip(0);

				// Set compression options.
				Tex->SRGB = TexParams.bSRGB;
				Tex->CompressionSettings = TexParams.CompressionSettings;
				Tex->MipGenSettings = TexParams.MipGenSettings;
				Tex->CompressionNoAlpha = true;
				Tex->DeferCompression = TexParams.bDeferCompression;
				
				
				Tex->PostEditChange();
				Tex->MarkPackageDirty();
				
				return Tex;
			}
		}
	}

	return nullptr;
}

bool UMatFactory::GetVector(const FString& Str, const int32 CompsNum, TArray<float>& OutVector)
{
	TArray<const TCHAR*> Delimiters = { TEXT("("), TEXT(")"), TEXT(",") };
	TArray<FString> Components;

	Str.ParseIntoArray(Components, Delimiters.GetData(), Delimiters.Num(), true);

	if (Components.Num() != CompsNum) return false;

	for (const FString& Component : Components)
	{
		OutVector.Add(FCString::Atof(*Component));
	}
	return true;
}

bool UMatFactory::LinkNodes(TArray<FString> Params)
{
	if (Params.Num() != 3) return false;

	if (Nodes.Contains(*Params[1]) && MaterialInputs.Contains(*Params[2]))
	{
		MaterialInputs[*Params[2]]->Expression = Nodes[*Params[1]];
		return true;
	}

	return false;
}

bool UMatFactory::AddNode(TArray<FString> Params, UMaterial* Material)
{
	if (!NodesFunction.Contains(*Params[0])) return false;
	auto NodeFunction = NodesFunction[*Params[0]];
	Params.RemoveAt(0);

	if (UMaterialExpression* MatExp = (this->*NodeFunction)(Params, Material))
	{
		Material->Expressions.Add(MatExp);
		Nodes.Emplace(Params[0], MatExp);
		return true;
	}

	return false;
}

void UMatFactory::InitNodesFunction()
{
	ADD_FUNCTION(VectorParameter)
	ADD_FUNCTION(TextureSample)
	ADD_FUNCTION(ScalarParameter)
	ADD_FUNCTION(Time)
	ADD_FUNCTION(Panner)
	ADD_FUNCTION(Constant2Vector)
	ADD_FUNCTION(Sine)
	ADD_FUNCTION(Add)
	ADD_FUNCTION(Subtract)
	ADD_FUNCTION(Divide)
}

void UMatFactory::InitMaterialInputs(UMaterial* Material)
{
	ADD_INPUT(Material, BaseColor)
	ADD_INPUT(Material, Metallic)
	ADD_INPUT(Material, Specular)
	ADD_INPUT(Material, Roughness)
	ADD_INPUT(Material, EmissiveColor)
	ADD_INPUT(Material, Normal)
	ADD_INPUT(Material, WorldPositionOffset)
	ADD_INPUT(Material, AmbientOcclusion)
}
