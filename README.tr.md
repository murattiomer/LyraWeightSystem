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

Component üzerinde `OverweightEffectClass`'ı ayarla. Component bu efekti `Weight`, `MaxWeight`'i aştığı sürece uygular ve `Weight` tekrar altına düştüğünde kaldırır. Varsayılan efekt yalnızca `Status.Overweight` tag'ini ekler — bu tag'e tepki vermek (hareket cezası, UI uyarısı) oyuna bırakılmıştır.

![Overweight efekti bir tag ekler](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_2.png)

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
    return 100.f;
}

FOnWeightContributionChanged& ULyraInventoryManagerComponent::GetOnWeightContributionChanged()
{
    return OnWeightContributionChangedDelegate;
}
```

Yukarıdaki `100.f` bir yer tutucudur — bu component'in taşıdığı şeyin gerçek toplamıyla değiştir.

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

Bir HUD widget'ı construct anında bu delegate'lere bağlanır ve onlar tetiklendiğinde kendini yeniler. Construct'ta yenilemeyi bir kez de doğrudan çağırır ki widget, bir sonraki değişim gelmeden önce güncel değeri göstersin:

![Widget ağırlık delegate'lerine bağlanır](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_9.png)

Yenileme, component'i doğrudan okur — `GetWeightNormalized()` ilerleme çubuğunu besler, `GetCurrentWeight()` ve `GetMaxWeight()` etiketi biçimlendirir, `IsOverweight()` ise çubuğun rengini seçer:

![Widget yenilemesi component'i okur](https://raw.githubusercontent.com/omergfx28/LyraWeightSystem/main/Images/Screenshot_10.png)

## Planlanan

- **Gameplay effect üzerinden hareket cezası.** Hızı ayarlayan bir gameplay ability yerine, `Status.Overweight` tag'ine bağlı bir gameplay effect doğrudan bir hareket çarpanı uygular. Bu, hareket hızının efektin değiştirebileceği bir attribute üzerinden yönetilmesine bağlıdır.
- **Gerçek item'lardan envanter katkısı.** Referans contributor sabit bir değer döndürür; gerçek olanı taşıdığı item'ların ağırlığını toplar.