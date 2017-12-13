// Copyright 2017, Institute for Artificial Intelligence - University of Bremen


#include "ROSActorManager.h"
#include "ROSMarkerArraySubscriber.h"


// Sets default values
AROSActorManager::AROSActorManager():xWorldOffset(-270),yWorldOffset(70),zWorldOffset(0)
{
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
  PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AROSActorManager::BeginPlay()
{
  Super::BeginPlay();

  UE_LOG(LogTemp, Log, TEXT("[ARobCoGGameModeBase::BeginPlay()]"));
  Handler = MakeShareable<FROSBridgeHandler>(new FROSBridgeHandler(TEXT("192.168.100.199"), 9090));

  AddNewObjectServiceServer =
    MakeShareable<FROSUpdateActorPositionServiceServer>(new FROSUpdateActorPositionServiceServer(TEXT("update_objects")));

  Subscriber = MakeShareable<FROSMarkerArraySubscriber>(new FROSMarkerArraySubscriber(TEXT("/RoboSherlock/markers")));

  Handler->AddServiceServer(AddNewObjectServiceServer);
  Handler->AddSubscriber(Subscriber);

  Handler->Connect();
  UE_LOG(LogTemp, Log, TEXT("[ARobCoGGameModeBase::BeginPlay()] Websocket server connected."));

}


// Called every frame
void AROSActorManager::Tick(float DeltaTime)
{  
  Handler->Render();
  TNamePosePair posePair;

  //see if camera needs moving;
  if(AddNewObjectServiceServer->MessageQueue.Dequeue(posePair))
  {
    UE_LOG(LogTemp, Log, TEXT("Key: [%s]"), *posePair.Key);
    UE_LOG(LogTemp, Log, TEXT("Value: [%s]"), *posePair.Value.ToString());
    for(TActorIterator<ACameraActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
      if(ActorItr->GetName().Contains("RGBDCamera"))
      {
        FROSBridgeMsgGeometrymsgsPoseStamped poseStamped;
        poseStamped.SetHeader(posePair.Value.GetHeader());
        poseStamped.SetPose(posePair.Value.GetPose());
        UE_LOG(LogTemp, Log, TEXT("Found actor with given name. Moving it"));
        FVector trans(-poseStamped.GetPose().GetPosition().GetX() * 100 + xWorldOffset,
                      poseStamped.GetPose().GetPosition().GetY()  * 100 + yWorldOffset,
                      poseStamped.GetPose().GetPosition().GetZ()  * 100 + zWorldOffset);
        FQuat quat(-poseStamped.GetPose().GetOrientation().GetX(), poseStamped.GetPose().GetOrientation().GetY(),
                   poseStamped.GetPose().GetOrientation().GetZ(), -poseStamped.GetPose().GetOrientation().GetW());

        FRotator hack(90, 90, 0); //this seemed to work...why?
        ActorItr->SetActorLocationAndRotation(trans, quat * hack.Quaternion());
      }
    }
  }

  //loop through markers and spawn
  FROSBridgeMsgVisualizationmsgsMarker marker;
  while(Subscriber->ObjectsToUpdate.Dequeue(marker))
  {
    for(TActorIterator<AStaticMeshActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
      FString meshResource = marker.GetMeshResource();
      int32 idxFrom = meshResource.Len() - meshResource.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
      meshResource = meshResource.Right(idxFrom - 1);
      int32 idxTo = meshResource.Find(TEXT("."));
      meshResource = meshResource.Left(idxTo);
      FString actorName = Subscriber->nameMapping[meshResource];
      if(ActorItr->GetName().Equals(actorName))
      {
        UE_LOG(LogTemp, Log, TEXT("Marker Mesh: %s"), *meshResource);
        UE_LOG(LogTemp, Log, TEXT("Found actor with given name [%s]"), *actorName);
        FROSBridgeMsgGeometrymsgsPose pose = marker.GetPose();
        FVector trans(-pose.GetPosition().GetX() * 100 + xWorldOffset,
                      pose.GetPosition().GetY()  * 100 + yWorldOffset,
                      pose.GetPosition().GetZ()  * 100 + zWorldOffset);
        FQuat quat(-pose.GetOrientation().GetX(), pose.GetOrientation().GetY(),
                   pose.GetOrientation().GetZ(), -pose.GetOrientation().GetW());

        UE_LOG(LogTemp, Log, TEXT("Marker trans: %f %f %f"), trans[0], trans[1], trans[2]);
        ActorItr->SetActorLocationAndRotation(trans, quat);
        ActorItr->SetActorHiddenInGame(false);
      }

    }
  }
  UpdateComponentTransforms();
  Super::Tick(DeltaTime);
}

void AROSActorManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Handler->Disconnect();
  Super::EndPlay(EndPlayReason);
  UE_LOG(LogTemp, Log, TEXT("[%s]]End play!"), *FString(__FUNCTION__));

}
