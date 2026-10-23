#include "NoirScriptLibrary.h"

#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"

int32 UNoirScriptLibrary::FixCollectionIds(UMaterialParameterCollection* Collection)
{
	if (!Collection) return 0;
	TSet<FGuid> Seen;
	int32 Fixed = 0;
	auto Fix = [&Seen, &Fixed](FCollectionParameterBase& P)
	{
		if (!P.Id.IsValid() || Seen.Contains(P.Id))
		{
			P.Id = FGuid::NewGuid();
			++Fixed;
		}
		Seen.Add(P.Id);
	};
	for (FCollectionScalarParameter& P : Collection->ScalarParameters) Fix(P);
	for (FCollectionVectorParameter& P : Collection->VectorParameters) Fix(P);
	if (Fixed)
	{
		Collection->PostEditChange();
		Collection->MarkPackageDirty();
	}
	return Fixed;
}

bool UNoirScriptLibrary::BindCollectionParameter(UMaterialExpression* Node, UMaterialParameterCollection* Collection, FName Parameter)
{
	UMaterialExpressionCollectionParameter* C = Cast<UMaterialExpressionCollectionParameter>(Node);
	if (!C || !Collection) return false;
	const FGuid Id = Collection->GetParameterId(Parameter);
	if (!Id.IsValid()) return false;
	C->Collection = Collection;
	C->ParameterName = Parameter;
	C->ParameterId = Id;
	return true;
}
