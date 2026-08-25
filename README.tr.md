# Lyra Weight System

[English](README.md) | [Türkçe](README.tr.md)

Lyra için GAS ve Modular Gameplay üzerine kurulu, modüler bir taşıma ağırlığı sistemi. Bir pawn'ın taşıdığı ağırlığı maksimum kapasiteye karşı takip eder ve pawn aşırı yüklendiğinde bir gameplay effect uygular.

Çekirdek; envanter, item ya da ekipman hakkında hiçbir şey bilmez. Ağırlık katan her şey kendini küçük bir arayüz üzerinden tanıtır, böylece sistem düzenlenmeye gerek kalmadan herhangi bir sayıda kaynakla birleşir. Bir Game Feature üzerinden eklenmek üzere tasarlanmıştır; bu sayede ağırlık yalnızca onu isteyen deneyimlerde var olur ve temel karaktere hiç dokunmaz.

## İçindekiler

| Sınıf | Görevi |
|---|---|
| `ULyraWeightSet` | `Weight` ve `MaxWeight` attribute'larını tutan attribute set. |
| `ULyraWeightComponent` | Katkı sağlayanları toplar, ağırlıklarını hesaplar, toplamı yazar ve overweight efektini uygular. |
| `ILyraWeightContributor` | Ağırlık katan her şeyin implement ettiği arayüz. |

## Nasıl bir araya geliyor

Weight component, owner'ı üzerindeki her `ILyraWeightContributor`'ı bulur, her birinin değişim delegate'ine abone olur ve katkılarını `Weight` attribute'unda toplar. Somut bir contributor tipine hiçbir zaman referans vermez, bu yüzden yeni bir ağırlık kaynağı eklemek component'te hiçbir değişiklik gerektirmez. Ağırlık sunucuda hesaplanır ve attribute UI için istemcilere replike olur.

![Çalışma zamanı veri akışı](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_8.png)

Sistem kendi kendini başlatır. Bir Game Feature, bir ability set grant eder (bu da weight set'i ASC'ye ekler) ve weight component'i ekler; pawn extension component ASC'nin hazır olduğunu bildirdiğinde, component contributor'larına abone olur ve ilk toplamı hesaplar.

![Başlatma sırası](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_7.png)

## Gereksinimler

Lyra (modül `Source/LyraGame/WeightSystem` altında yaşar), ayrıca her Lyra projesinde varsayılan olarak etkin olan `GameplayAbilities` ve `ModularGameplay` eklentileri.

## Kurulum

### 1. Weight set'i bir ability set üzerinden grant et

`ULyraWeightSet`'i bir ability set'e (`ULyraAbilitySet`) **Granted Attributes** altından ekle. Bu ability set'i grant eden Game Feature, weight set'i pawn'ın ability system'ine ekler — tıpkı diğer tüm Lyra attribute set'lerinin grant edildiği gibi.

![Ability set içinde weight set](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_1.png)

### 2. Weight component'i ekle

`ULyraWeightComponent`'i pawn'a bir Game Feature Action (Add Components) ile, `ALyraCharacter`'ı hedefleyerek ekle. Bu, ağırlığın yalnızca özelliği etkinleştiren deneyimlerde katmanlanmasını sağlar. Manuel bir başlatma çağrısı gerekmez.

![Weight component ekleme](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_6.png)

### 3. Overweight efektini ata

Component üzerinde `OverweightEffectClass`'ı ayarla. Component bu efekti `Weight`, `MaxWeight`'i aştığı sürece uygular ve `Weight` tekrar altına düştüğünde kaldırır. Efekt `Status.Overweight` tag'ini ekler ve doğrudan hareket hızı attribute'unu (`LyraMovementSet.MovementSpeed`) değiştirir.

![Overweight efekti tag ekler ve hareket hızını değiştirir](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_2.png)

## Ağırlık kaynağı ekleme

`ILyraWeightContributor`'ı, içeriği taşıma ağırlığına sayılması gereken herhangi bir component'e implement et — bir envanter, bir ekipman yöneticisi, bir sırt çantası. Weight component, pawn üzerindeki her contributor'ın `GetWeightContribution()` değerini toplar ve herhangi bir contributor değişim delegate'ini broadcast ettiğinde yeniden hesaplar.

Önce arayüzü implement et ve delegate'i tut. Header'da:

```cpp
/**
 * Manages an inventory
 */
UCLASS(MinimalAPI, BlueprintType)
class ULyraInventoryManagerComponent : public UActorComponent, public ILyraWeightContributor
{
    GENERATED_BODY()

public:
    virtual float GetWeightContribution() const override;
    virtual FOnWeightContributionChanged& GetOnWeightContributionChanged() override;

private:
    UPROPERTY(Replicated)
    FLyraInventoryList InventoryList;

    FOnWeightContributionChanged OnWeightContributionChangedDelegate;
};
```

Ardından güncel toplamı döndür ve delegate'i geri ver. cpp'de:

```cpp
float ULyraInventoryManagerComponent::GetWeightContribution() const
{
    float TotalWeight = 0.f;

    for (const FLyraInventoryEntry& Entry : InventoryList.Entries)
    {
        if (Entry.Instance)
        {
            // Test: each item counts as 10 weight per stack.
            TotalWeight += 10.f * Entry.StackCount;
        }
    }

    return TotalWeight;
}

FOnWeightContributionChanged& ULyraInventoryManagerComponent::GetOnWeightContributionChanged()
{
    return OnWeightContributionChangedDelegate;
}
```

Son olarak, içeriği değiştiren her authoritative yoldan `OnWeightContributionChanged`'i broadcast et ki weight component yeniden hesaplaması gerektiğini bilsin:

```cpp
ULyraInventoryItemInstance* ULyraInventoryManagerComponent::AddItemDefinition(TSubclassOf<ULyraInventoryItemDefinition> ItemDef, int32 StackCount)
{
    ULyraInventoryItemInstance* Result = nullptr;
    if (ItemDef != nullptr)
    {
        Result = InventoryList.AddEntry(ItemDef, StackCount);

        if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && Result)
        {
            AddReplicatedSubObject(Result);
        }

        // Sunucu tarafında, içerik değiştiği her yerde:
        OnWeightContributionChangedDelegate.Broadcast();
    }
    return Result;
}

void ULyraInventoryManagerComponent::AddItemInstance(ULyraInventoryItemInstance* ItemInstance)
{
    InventoryList.AddEntry(ItemInstance);
    if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
    {
        AddReplicatedSubObject(ItemInstance);

        // Sunucu tarafında, içerik değiştiği her yerde:
        OnWeightContributionChangedDelegate.Broadcast();
    }
}

void ULyraInventoryManagerComponent::RemoveItemInstance(ULyraInventoryItemInstance* ItemInstance)
{
    InventoryList.RemoveEntry(ItemInstance);

    if (ItemInstance && IsUsingRegisteredSubObjectList())
    {
        RemoveReplicatedSubObject(ItemInstance);

        // Sunucu tarafında, içerik değiştiği her yerde:
        OnWeightContributionChangedDelegate.Broadcast();
    }
}
```

Ağırlık sunucuda hesaplanır: component toplamı authority'de yazar ve `Weight` attribute'u GAS üzerinden istemcilere replike olur. İstemciler replike edilen değeri UI için okur; yeniden hesaplamazlar.

## Ağırlığı okuma

`ULyraWeightComponent` şunları sunar: `GetCurrentWeight()`, `GetMaxWeight()`, `GetWeightNormalized()` (0–1, ilerleme çubuğu için) ve `IsOverweight()`. Ayrıca `OnCurrentWeightChanged` ve `OnMaxWeightChanged` delegate'lerini broadcast eder; böylece UI her kare yoklama yapmak yerine ağırlık değiştiği anda güncellenebilir.

Bir HUD widget'ı delegate'leri güvenli bir şekilde yönetmek için pawn sahipliği (possession) değişimlerine bağlanır. Widget, `OnPossessedPawnChanged` event'ini dinleyerek yeni pawn'daki weight component'i bulur, eski pawn'ın component'inden delegate bağlantısını koparır ve yeni pawn'ın `OnCurrentWeightChanged` delegate'ine kendi özel event'ini bağlar.

![Widget pawn değişimlerine bağlanır](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_11.png)

UI yenileme mantığı delegate tetiklendiğinde component'i doğrudan okur. `GetWeightNormalized()` ilerleme çubuğunu besler, `GetCurrentWeight()` ve `GetMaxWeight()` metni biçimlendirir, `IsOverweight()` ise arayüzün rengini belirlemek için kullanılır:

![Widget yenilemesi component'i okur](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_12.png)

### Çalışma Zamanı Sonucu (Runtime Result)

Pawn spawn olduğunda, başlangıç ağırlığı mevcut envantere göre hesaplanır (örn. 10/100):

![Oyun içi HUD başlangıç ağırlık göstergesi](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_3.png)

Yerden yeni bir eşya veya silah alındığında değişim delegate'i tetiklenir, toplam ağırlık yeniden hesaplanır ve UI anlık olarak güncellenir (örn. 20/100):

![Silah alındıktan sonra güncellenen ağırlık göstergesi](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_4.png)