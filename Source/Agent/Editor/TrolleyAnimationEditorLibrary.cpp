// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/TrolleyAnimationEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_Root.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Engine/SkeletalMesh.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AnimGraphNode_ControlRig.h"

namespace
{
	const TCHAR* TrolleyAnimBlueprintPath = TEXT("/Game/ThirdPerson/VehicleAnimations/AgentTrolleyAnimInstance.AgentTrolleyAnimInstance");
	const TCHAR* QuinnSimpleMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple");

	UEdGraph* FindAnimGraph(UAnimBlueprint* AnimBlueprint)
	{
		if (!AnimBlueprint)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				return Graph;
			}
		}

		return nullptr;
	}

	template<typename TNodeType>
	TNodeType* FindFirstGraphNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		TArray<TNodeType*> Nodes;
		Graph->GetNodesOfClass<TNodeType>(Nodes);
		return Nodes.Num() > 0 ? Nodes[0] : nullptr;
	}

	UEdGraphPin* FindPosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const FName PreferredName = NAME_None)
	{
		if (!Node)
		{
			return nullptr;
		}

		if (PreferredName != NAME_None)
		{
			if (UEdGraphPin* PreferredPin = Node->FindPin(PreferredName, Direction))
			{
				if (UAnimationGraphSchema::IsPosePin(PreferredPin->PinType))
				{
					return PreferredPin;
				}
			}
		}

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && UAnimationGraphSchema::IsPosePin(Pin->PinType))
			{
				return Pin;
			}
		}

		return nullptr;
	}

	bool ConnectPosePins(UAnimationGraphSchema* Schema, UEdGraphPin* OutputPin, UEdGraphPin* InputPin)
	{
		if (!Schema || !OutputPin || !InputPin)
		{
			return false;
		}

		const bool bAlreadyLinked = OutputPin->LinkedTo.Contains(InputPin) && InputPin->LinkedTo.Contains(OutputPin);
		if (bAlreadyLinked)
		{
			return true;
		}

		OutputPin->BreakAllPinLinks();
		InputPin->BreakAllPinLinks();
		return Schema->TryCreateConnection(OutputPin, InputPin);
	}
}
#endif

bool UTrolleyAnimationEditorLibrary::FixTrolleyPostProcessAnimBlueprint()
{
#if !WITH_EDITOR
	return false;
#else
	UAnimBlueprint* TrolleyBlueprint = LoadObject<UAnimBlueprint>(nullptr, TrolleyAnimBlueprintPath);
	if (!TrolleyBlueprint)
	{
		return false;
	}

	UEdGraph* AnimGraph = FindAnimGraph(TrolleyBlueprint);
	if (!AnimGraph)
	{
		return false;
	}

	UAnimGraphNode_ControlRig* ControlRigNode = FindFirstGraphNode<UAnimGraphNode_ControlRig>(AnimGraph);
	UAnimGraphNode_Root* RootNode = FindFirstGraphNode<UAnimGraphNode_Root>(AnimGraph);
	if (!ControlRigNode || !RootNode)
	{
		return false;
	}

	UAnimGraphNode_LinkedInputPose* LinkedInputNode = FindFirstGraphNode<UAnimGraphNode_LinkedInputPose>(AnimGraph);
	if (!LinkedInputNode)
	{
		const FVector2D SpawnPosition(ControlRigNode->NodePosX - 400.0f, ControlRigNode->NodePosY);
		LinkedInputNode = FEdGraphSchemaAction_K2NewNode::SpawnNode<UAnimGraphNode_LinkedInputPose>(AnimGraph, SpawnPosition, EK2NewNodeFlags::None);
	}

	LinkedInputNode = FindFirstGraphNode<UAnimGraphNode_LinkedInputPose>(AnimGraph);
	if (!LinkedInputNode)
	{
		return false;
	}

	UAnimationGraphSchema* Schema = GetMutableDefault<UAnimationGraphSchema>();
	UEdGraphPin* LinkedInputOutputPin = FindPosePin(LinkedInputNode, EGPD_Output);
	UEdGraphPin* ControlRigInputPin = FindPosePin(ControlRigNode, EGPD_Input, TEXT("Source"));
	UEdGraphPin* ControlRigOutputPin = FindPosePin(ControlRigNode, EGPD_Output);
	UEdGraphPin* RootInputPin = FindPosePin(RootNode, EGPD_Input, TEXT("Result"));
	if (!LinkedInputOutputPin || !ControlRigInputPin || !ControlRigOutputPin || !RootInputPin)
	{
		return false;
	}

	const bool bLinkedInputConnected = ConnectPosePins(Schema, LinkedInputOutputPin, ControlRigInputPin);
	const bool bRootConnected = ConnectPosePins(Schema, ControlRigOutputPin, RootInputPin);
	if (!bLinkedInputConnected || !bRootConnected)
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TrolleyBlueprint);
	FKismetEditorUtilities::CompileBlueprint(TrolleyBlueprint);

	USkeletalMesh* QuinnSimpleMesh = LoadObject<USkeletalMesh>(nullptr, QuinnSimpleMeshPath);
	if (QuinnSimpleMesh)
	{
		if (UClass* GeneratedClass = TrolleyBlueprint->GeneratedClass.Get())
		{
			if (GeneratedClass->IsChildOf(UAnimInstance::StaticClass()))
			{
				QuinnSimpleMesh->Modify();
				QuinnSimpleMesh->SetPostProcessAnimBlueprint(GeneratedClass);
				QuinnSimpleMesh->MarkPackageDirty();
			}
		}
	}

	TArray<UPackage*> PackagesToSave;
	if (UPackage* BlueprintPackage = TrolleyBlueprint->GetPackage())
	{
		PackagesToSave.Add(BlueprintPackage);
	}

	if (QuinnSimpleMesh)
	{
		if (UPackage* MeshPackage = QuinnSimpleMesh->GetPackage())
		{
			PackagesToSave.AddUnique(MeshPackage);
		}
	}

	if (PackagesToSave.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
	}

	return true;
#endif
}
