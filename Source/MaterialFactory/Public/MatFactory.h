// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"

#include "MatFactory.generated.h"

#define ADD_INPUT(Material, InputName) MaterialInputs.Emplace(#InputName, &Material->InputName);

#define ADD_FUNCTION(NodeName) NodesFunction.Emplace(#NodeName, &UMatFactory::##Add<UMaterialExpression##NodeName>);

#define ADD_NODE_INPUT(Node, NodeName, InputName) MaterialInputs.Emplace(*(NodeName+#InputName), &Node->InputName);

#define SET_OPNODE(Node, Attribute, Param) \
if (Param.IsNumeric()) Node->Const##Attribute = FCString::Atof(*Param); \
else if (Nodes.Contains(*Param)) Node->Attribute.Expression = Nodes[*Param]; \
else return nullptr;

/**
 * 
 */
UCLASS()
class EDITORPLUGIN_API UMatFactory : public UFactory
{
	GENERATED_BODY()

	const TCHAR* CONNECT_CMD = TEXT("Connect");

	TMap<FName, UMaterialExpression* (UMatFactory::*)(TArray<FString>& Params, UObject* Parent)> NodesFunction;
	TMap<FName, FExpressionInput*> MaterialInputs;
	TMap<FName, UMaterialExpression*> Nodes;
	
	void InitNodesFunction();
	void InitMaterialInputs(UMaterial* Material);

public:
	UMatFactory();

	UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName,
							   EObjectFlags Flags, const FString& Filename,
							   const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	UMaterial* CreateMaterial(UObject* Package, FName MaterialName, EObjectFlags Flags, const FString& Filepath);	

	UTexture2D* LoadTexture(UObject* Package, const FString& FilePath);

	bool GetVector(const FString& Str, const int32 CompsNum, TArray<float>& OutVector);

	bool LinkNodes(TArray<FString> Params);

	bool AddNode(TArray<FString> Params, UMaterial* Material);

	template<typename T>
	UMaterialExpression* Add(TArray<FString>& Params, UObject* Parent)
	{
		return nullptr;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionVectorParameter>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionVectorParameter* VectorParameter = NewObject<UMaterialExpressionVectorParameter>(Parent);
		VectorParameter->SetParameterName(*Params[0]);
		
		TArray<float> Color;
		if (!GetVector(Params[1], 4, Color)) return nullptr;

		VectorParameter->DefaultValue = FLinearColor(Color[0], Color[1], Color[2], Color[3]);
		return VectorParameter;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionTextureSample>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionTextureSample* TexSample = NewObject<UMaterialExpressionTextureSample>(Parent);
		TexSample->SetParameterName(*Params[0]);

		if (UTexture2D* Tex = LoadTexture(Parent, Params[1]))
		{
			TexSample->Texture = Tex;
			ADD_NODE_INPUT(TexSample, Params[0], Coordinates)
			return TexSample;
		}

		return nullptr;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionScalarParameter>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionScalarParameter* ScalarParameter = NewObject<UMaterialExpressionScalarParameter>(Parent);
		ScalarParameter->SetParameterName(*Params[0]);
		ScalarParameter->DefaultValue = FCString::Atof(*Params[1]);
		return ScalarParameter;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionMultiply>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionMultiply* Multiplier = NewObject<UMaterialExpressionMultiply>(Parent);
		Multiplier->SetParameterName(*Params[0]);

		SET_OPNODE(Multiplier, A, Params[1])
		SET_OPNODE(Multiplier, B, Params[2])

		return Multiplier;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionTime>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionTime* Time = NewObject<UMaterialExpressionTime>(Parent);
		Time->SetParameterName(*Params[0]);
		return Time;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionPanner>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionPanner* Panner = NewObject<UMaterialExpressionPanner>(Parent);
		Panner->SetParameterName(*Params[0]);

		ADD_NODE_INPUT(Panner, Params[0], Coordinate)
		ADD_NODE_INPUT(Panner, Params[0], Speed)
		ADD_NODE_INPUT(Panner, Params[0], Time)

		return Panner;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionConstant2Vector>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionConstant2Vector* Vector = NewObject<UMaterialExpressionConstant2Vector>(Parent);
		Vector->SetParameterName(*Params[0]);
		
		TArray<float> Components;
		if (!GetVector(Params[1], 2, Components)) return nullptr;

		Vector->R = Components[0];
		Vector->G = Components[1];
		return Vector;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionSine>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionSine* Sine = NewObject<UMaterialExpressionSine>(Parent);
		Sine->SetParameterName(*Params[0]);
		
		if (Nodes.Contains(*Params[1]))
		{
			Sine->Input.Expression = Nodes[*Params[1]];
		}

		return nullptr;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionAdd>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionAdd* Adder = NewObject<UMaterialExpressionAdd>(Parent);
		Adder->SetParameterName(*Params[0]);
		
		SET_OPNODE(Adder, A, Params[1])
		SET_OPNODE(Adder, B, Params[2])

		return Adder;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionSubtract>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionSubtract* Subtracter = NewObject<UMaterialExpressionSubtract>(Parent);
		Subtracter->SetParameterName(*Params[0]);

		SET_OPNODE(Subtracter, A, Params[1])
		SET_OPNODE(Subtracter, B, Params[2])

		return Subtracter;
	}

	template<>
	UMaterialExpression* Add<UMaterialExpressionDivide>(TArray<FString>& Params, UObject* Parent)
	{
		UMaterialExpressionDivide* Divisor = NewObject<UMaterialExpressionDivide>(Parent);
		Divisor->SetParameterName(*Params[0]);

		SET_OPNODE(Divisor, A, Params[1])
		SET_OPNODE(Divisor, B, Params[2])

		return Divisor;
	}
};
