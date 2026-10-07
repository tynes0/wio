# Wio gömülü method referansı

Bu belge, `.wio` std dosyalarında gövdesi/deklarasyonu görünmeyen, derleyicinin
doğrudan tanıdığı üye methodları listeler. Mevcut kaynak ağacındaki semantik
imzaları esas alır; gelecekte planlanan API'leri veya her backend için bir
destek/parite garantisini ifade etmez.

`std::Vector`, `std::Span`, `std::Option`, `std::Result`, buffer, pool ve diğer
std tiplerinin kaynakta tanımlı methodları bu katalogdan ayrıdır. Onlar için
[std referansına](./WIO_STD.md) ve ilgili `std/*.wio` dosyasına bakılabilir.

## İçindekiler

- [Okuma kuralları](#okuma-kuralları)
- [string](#string)
- [text](#text)
- [Array: T[] ve [T; N]](#array)
- [Dictionary: Dict ve Tree](#dictionary)
- [enum](#enum)
- [flagset](#flagset)
- [Async: coroutine](#async)
- [Generic/value pack üyeleri](#pack)
- [Kısa örnekler](#örnekler)
- [Kaynaklar ve bakım](#kaynaklar-ve-bakım)

<a id="okuma-kuralları"></a>

## Okuma kuralları

- İmzalar `Method(parametre: Tip) -> DönüşTipi` şeklinde yazılır.
  Parametre adları açıklayıcı etiketlerdir; named-argument desteği anlamına gelmez.
- Method adları büyük/küçük harfe duyarlıdır: `ToI32`, `Count`, `GetOr` gibi.
- `void`, bir sonuç değeri dönmediğini belirtir.
- `T`: eleman tipi; `K`: anahtar tipi; `V`: dictionary değer tipi;
  `N`: sabit eleman sayısı; `A`: alıcının tam array tipi;
  `D`: alıcının tam dictionary tipi; `E`: enum tipi; `F`: flagset tipi;
  `U`: enum'un tanımlanmış temel integer tipi.
- **`mut` gerekli** tablolarındaki methodlar değiştirilebilir bir alıcı ister.
  Salt-okunur `view` veya değiştirilemez bir değişken üzerinden çağrılamazlar.
  `ref`/`view`/alias kullanımı yeni bir method kümesi oluşturmaz; alttaki tip ve
  erişim hakkı esas alınır. Nullable alıcı önce non-null olarak daraltılmalıdır.
- `Get` ile `At` aynı değildir: `Get` bulunamayan değeri `std::Option` ile
  temsil eder, `At` doğrudan değer ister. `GetOr` verilen yedeği döndürür.
  `Option` dönen imzalar `std::Option` deklarasyonunun kullanılabilir olmasını
  gerektirir; builtin std kapatıldığında bu bağımlılık ortadan kalkmaz.

<a id="string"></a>

## string

`string` byte dizisidir. UTF-8 metin taşıyabilir ama `Count`, index, `Slice`,
`First`/`Last`, `Reverse` ve `char` işlemleri Unicode karakter/grapheme değil,
byte üzerinden çalışır. Unicode metin işlemleri için `text` kullanılmalıdır.

### Okuma ve arama — mut gerekli değil

| İmza | Açıklama |
| --- | --- |
| `Count() -> usize` | Byte sayısı. |
| `Empty() -> bool` | Boş mu? |
| `Contains(value: string) -> bool` | Alt string var mı? |
| `Contains(value: char) -> bool` | Byte/char var mı? |
| `ContainsChar(value: char) -> bool` | Char aramasının açık isimli karşılığı. |
| `StartsWith(prefix: string) -> bool` | Önek kontrolü. |
| `EndsWith(suffix: string) -> bool` | Sonek kontrolü. |
| `IndexOf(value: string) -> isize` | İlk eşleşmenin byte index'i; yoksa `-1`. |
| `IndexOf(value: char) -> isize` | İlk char eşleşmesi; yoksa `-1`. |
| `LastIndexOf(value: string) -> isize` | Son eşleşmenin byte index'i; yoksa `-1`. |
| `LastIndexOf(value: char) -> isize` | Son char eşleşmesi; yoksa `-1`. |
| `IndexOfChar(value: char) -> isize` | Char için ilk index; yoksa `-1`. |
| `LastIndexOfChar(value: char) -> isize` | Char için son index; yoksa `-1`. |
| `First() -> char` | İlk byte; boş string hata durumudur. |
| `Last() -> char` | Son byte; boş string hata durumudur. |
| `Get(index: usize) -> std::Option<char>` | Index geçerliyse `Some`, değilse `None`. |
| `At(index: usize) -> char` | Doğrudan byte; geçersiz index hata durumudur. |
| `GetOr(index: usize, fallback: char) -> char` | Geçersiz index'te yedek char. |

### Yeni değer üreten işlemler — mut gerekli değil

| İmza | Açıklama |
| --- | --- |
| `Slice(start: usize) -> string` | Başlangıçtan sona kadar. |
| `Slice(start: usize, count: usize) -> string` | En fazla `count` byte; ikinci argüman bitiş index'i değildir. |
| `SliceFrom(start: usize) -> string` | Başlangıçtan sona kadar. |
| `Take(count: usize) -> string` | Baştan en fazla `count` byte. |
| `Skip(count: usize) -> string` | Baştaki byte'ları atlar. |
| `Left(count: usize) -> string` | Soldan en fazla `count` byte. |
| `Right(count: usize) -> string` | Sağdan en fazla `count` byte. |
| `Trim() -> string` | İki uçtaki whitespace'i temizler. |
| `TrimStart() -> string` | Başlangıçtaki whitespace'i temizler. |
| `TrimEnd() -> string` | Sondaki whitespace'i temizler. |
| `ToLower() -> string` | Byte tabanlı küçük harf dönüşümü; tam Unicode case folding değildir. |
| `ToUpper() -> string` | Byte tabanlı büyük harf dönüşümü. |
| `Replace(oldValue: string, newValue: string) -> string` | Eşleşmeleri değiştirir. |
| `ReplaceFirst(oldValue: string, newValue: string) -> string` | İlk eşleşmeyi değiştirir. |
| `Repeat(count: usize) -> string` | Tekrar edilmiş string. |
| `Split(separator: string) -> string[]` | Ayırıcıya göre parçalar. |
| `Lines() -> string[]` | Satırlara ayırır. |
| `PadLeft(width: usize) -> string` | Soldan boşlukla en az verilen byte genişliğine tamamlar. |
| `PadLeft(width: usize, fill: char) -> string` | Soldan verilen char ile tamamlar. |
| `PadRight(width: usize) -> string` | Sağdan boşlukla tamamlar. |
| `PadRight(width: usize, fill: char) -> string` | Sağdan verilen char ile tamamlar. |
| `Reversed() -> string` | Byte sırası ters çevrilmiş yeni string. |

Slice/take/skip işlemleri mevcut uzunluğa kırpılır; fazladan eleman üretmez.
UTF-8 kod noktalarının ortasından kesmek veya byte sırasını ters çevirmek,
geçerli Unicode metni geçersiz UTF-8 byte'larına dönüştürebilir.

### Tip dönüşümleri — mut gerekli değil

| Parametresiz imza | Taban alan overload |
| --- | --- |
| `ToI8() -> i8` | `ToI8(base: i32) -> i8` |
| `ToI16() -> i16` | `ToI16(base: i32) -> i16` |
| `ToI32() -> i32` | `ToI32(base: i32) -> i32` |
| `ToI64() -> i64` | `ToI64(base: i32) -> i64` |
| `ToU8() -> u8` | `ToU8(base: i32) -> u8` |
| `ToU16() -> u16` | `ToU16(base: i32) -> u16` |
| `ToU32() -> u32` | `ToU32(base: i32) -> u32` |
| `ToU64() -> u64` | `ToU64(base: i32) -> u64` |
| `ToISize() -> isize` | `ToISize(base: i32) -> isize` |
| `ToUSize() -> usize` | `ToUSize(base: i32) -> usize` |

| İmza | Açıklama |
| --- | --- |
| `ToF32() -> f32` | Floating-point parse. |
| `ToF64() -> f64` | Floating-point parse. |
| `ToBool() -> bool` | Bool parse. |

Parametresiz integer dönüşümleri taban `10` kullanır. Bu methodlar
`Option`/`Result` döndürmez; geçersiz parse veya aralık aşımı hata durumudur.
Kontrollü parse için `std::convert` kaynakta tanımlı API'lerine bakılmalıdır.

### Yerinde değiştirme — mut gerekli

| İmza | Açıklama |
| --- | --- |
| `Append(value: string) -> void` | Sona string ekler. |
| `Push(value: char) -> void` | Sona bir byte ekler. |
| `Insert(index: usize, value: string) -> void` | Verilen konuma ekler. |
| `Erase(start: usize, count: usize) -> void` | Verilen aralığı siler. |
| `Clear() -> void` | İçeriği boşaltır. |
| `Reverse() -> void` | Byte sırasını yerinde ters çevirir. |
| `ReplaceInPlace(oldValue: string, newValue: string) -> void` | Eşleşmeleri yerinde değiştirir. |
| `TrimInPlace() -> void` | İki uçtaki whitespace'i yerinde temizler. |
| `ToLowerInPlace() -> void` | Yerinde küçük harf dönüşümü. |
| `ToUpperInPlace() -> void` | Yerinde büyük harf dönüşümü. |

`string` için gömülü `Reserve` veya `Capacity` methodu kayıtlı değildir.

<a id="text"></a>

## text

`text`, doğrulanmış UTF-8 Unicode değeridir; literal biçimi `u"Merhaba 🌍"`.
Aşağıdaki methodların hiçbiri değiştirilebilir alıcı gerektirmez.

| İmza | Açıklama |
| --- | --- |
| `Count() -> usize` | Unicode kod noktası sayısı. |
| `ByteCount() -> usize` | UTF-8 depolama boyutu. |
| `Empty() -> bool` | Boş mu? |
| `Slice(start: usize) -> text` | Kod noktası index'inden sona kadar. |
| `Slice(start: usize, count: usize) -> text` | Kod noktası index'i ve adediyle kesit. |
| `ToString() -> string` | UTF-8 byte string üretir. |
| `Utf8() -> string` | `ToString` ile aynı gömülü dönüşüm. |
| `Contains(value: text) -> bool` | Alt text araması. |
| `StartsWith(prefix: text) -> bool` | Önek kontrolü. |
| `EndsWith(suffix: text) -> bool` | Sonek kontrolü. |
| `GraphemeCount() -> usize` | Grapheme kümesi sayısı. |
| `SliceGraphemes(start: usize, count: usize) -> text` | Grapheme index'i ve adediyle kesit. |
| `DisplayWidth() -> usize` | Unicode genişlik kurallarına göre görüntüleme sütunu hesabı; font piksel genişliği değildir. |
| `CaseFold() -> text` | Unicode case folding. |
| `CodePoints() -> u32[]` | Kod noktalarının sayısal değerleri. |
| `Graphemes() -> text[]` | Grapheme parçaları. |

Ek erişim: `value[index]` salt-okunur, tek kod noktalı bir **`text`** döndürür;
`u32` veya `char` döndürmez. Sayısal kod noktaları için `CodePoints()` kullanılır.
Kod noktası ile grapheme aynı şey değildir: birleşen işaretler ve bazı emoji
dizileri birden çok kod noktasından oluşur. Şu an `text` üzerinde gömülü
`Get`, `At`, `Append`, `Trim` veya `ToLower` methodu yoktur.

<a id="array"></a>

## Array: T[] ve [T; N]

`T[]` dinamik, `[T; N]` sabit boyutlu array'dir. Aşağıda `A`, alıcının tam
tipidir. Örneğin `[i32; 3]` üzerinde `Clone() -> [i32; 3]`,
`Slice(...) -> i32[]` olur.

### İki array türünde ortak — mut gerekli değil

| İmza | Koşul / açıklama |
| --- | --- |
| `Count() -> usize` | Eleman sayısı. |
| `Empty() -> bool` | Boş mu? |
| `Capacity() -> usize` | Dinamik kapasite; sabit array için `N`. |
| `Contains(value: T) -> bool` | `T` için intrinsic eşitlik desteği gerekir. |
| `IndexOf(value: T) -> isize` | Eşitlik desteği; ilk index, yoksa `-1`. |
| `LastIndexOf(value: T) -> isize` | Eşitlik desteği; son index, yoksa `-1`. |
| `First() -> T` | İlk eleman; boş array hata durumudur. |
| `Last() -> T` | Son eleman; boş array hata durumudur. |
| `Get(index: usize) -> std::Option<T>` | Geçerli index'te `Some`, değilse `None`. |
| `At(index: usize) -> T` | Geçersiz index hata durumudur. |
| `GetOr(index: usize, fallback: T) -> T` | Geçersiz index'te yedek değer. |
| `Clone() -> A` | Array değerini kopyalar; elemanları otomatik deep-clone etmez. |
| `Slice(start: usize) -> T[]` | Başlangıçtan sona kadar dinamik array. |
| `Slice(start: usize, count: usize) -> T[]` | En fazla `count` eleman. |
| `Take(count: usize) -> T[]` | Baştan elemanlar. |
| `Skip(count: usize) -> T[]` | Başlangıçtaki elemanları atlar. |
| `Concat(other: A) -> T[]` | Birleştirilmiş dinamik array. |
| `Reversed() -> A` | Sırası ters yeni array. |
| `Join(separator: string) -> string` | Yalnız `T = string`. |
| `Sorted() -> A` | Intrinsic sıralama desteği gerekir; sıralanmış kopya. |

`Concat` parametresi tam alıcı tipidir: `[T; N]` alıcıda diğer array de
`[T; N]` olmalıdır. Slice/take/skip uzunluğu mevcut elemana göre kırpılır.
Bu kesitler `Span`/borrow view değil, dinamik array değeri döndürür.

### İki array türünde ortak — mut gerekli

| İmza | Koşul / açıklama |
| --- | --- |
| `Fill(value: T) -> void` | Mevcut elemanların değerini değiştirir. |
| `Reverse() -> void` | Yerinde ters çevirir. |
| `Sort() -> void` | Intrinsic sıralama desteği gerekir; yerinde sıralar. |

### Yalnız dinamik T[] — mut gerekli

| İmza | Açıklama |
| --- | --- |
| `Push(value: T) -> void` | Sona ekler. |
| `PushFront(value: T) -> void` | Başa ekler. |
| `Pop() -> T` | Son elemanı çıkarıp döndürür; boş array hata durumudur. |
| `PopFront() -> T` | İlk elemanı çıkarıp döndürür; boş array hata durumudur. |
| `Insert(index: usize, value: T) -> void` | Konuma eleman ekler. |
| `RemoveAt(index: usize) -> void` | Konumdaki elemanı siler. |
| `Remove(value: T) -> bool` | Eşitlik desteği; ilk eşleşmeyi siler, silindi mi döndürür. |
| `Clear() -> void` | Array'i boşaltır. |
| `Extend(other: T[]) -> void` | Diğer array'in elemanlarını sona ekler. |
| `Reserve(capacity: usize) -> void` | Kapasite ayırır; eleman sayısını değiştirmez. |
| `ShrinkToFit() -> void` | Fazla kapasiteyi azaltma isteği. |

### Eşitlik ve sıralama sınırları

Mevcut resolver, eşitlik için `void`/bilinmeyen tip dışındaki primitive'leri,
`null`, generic parametreleri ve elemanları yine bu koşulu sağlayan array/dictionary
tiplerini kabul eder. Sıralama için primitive'leri ve generic parametreleri kabul
eder. Generic placeholder'ın kabul edilmesi her somut tipin runtime işlemini
desteklediği anlamına gelmez.

Bu kapı, kullanıcı tanımlı her `component`/`object`/enum için otomatik açılmaz;
bir karşılaştırma operatörü yazılması tek başına bu intrinsic resolver'ın
kısıtlarını değiştirmez. Comparator alan gömülü `Sort` overload'u yoktur.

<a id="dictionary"></a>

## Dictionary: Dict<K, V> ve Tree<K, V>

`Dict<K, V>` unordered, `Tree<K, V>` ordered dictionary'dir.
`D` alıcının tam tipidir; `Merge`/`Extend` aynı dictionary türünü alır.

### Ortak — alıcı için mut gerekli değil

| İmza | Açıklama |
| --- | --- |
| `Count() -> usize` | Kayıt sayısı. |
| `Empty() -> bool` | Boş mu? |
| `ContainsKey(key: K) -> bool` | Anahtar mevcut mu? |
| `ContainsValue(value: V) -> bool` | `V` için intrinsic eşitlik desteği gerekir. |
| `Get(key: K) -> std::Option<V>` | Anahtar varsa `Some`, yoksa `None`. |
| `At(key: K) -> V` | Anahtar yoksa hata durumudur. |
| `GetOr(key: K, fallback: V) -> V` | Anahtar yoksa yedek değer. |
| `TryGet(key: K, outValue: ref V) -> bool` | Bulursa output'a yazar ve `true` döner; bulamazsa output değişmez. |
| `Keys() -> K[]` | Anahtarların dinamik array'i. |
| `Values() -> V[]` | Değerlerin dinamik array'i. |
| `Clone() -> D` | Dictionary kopyası; değerlerdeki object handle'larını deep-clone etmez. |
| `Merge(other: D) -> D` | Birleştirilmiş kopya; çakışan anahtarda `other` değeri kullanılır. |

`TryGet` alıcı dictionary'yi değiştirmez ama `outValue` argümanı değiştirilebilir
referans olmalıdır. `Dict` için anahtar/değer iterasyon sırası garanti edilmez;
`Tree` anahtar sırasını kullanır.

### Ortak — mut gerekli

| İmza | Açıklama |
| --- | --- |
| `Set(key: K, value: V) -> void` | Ekler veya mevcut değeri değiştirir. |
| `GetOrAdd(key: K, fallback: V) -> V` | Mevcut değeri döndürür; yoksa yedeği ekler ve döndürür. |
| `Extend(other: D) -> void` | Yerinde birleştirir; çakışan anahtarda diğer değeri kullanır. |
| `Clear() -> void` | Tüm kayıtları siler. |
| `Remove(key: K) -> bool` | Anahtarı siler; silindi mi döndürür. |

### Yalnız Tree<K, V> — mut gerekli değil

| İmza | Açıklama |
| --- | --- |
| `FirstKey() -> K` | En küçük anahtar; boş Tree hata durumudur. |
| `FirstValue() -> V` | En küçük anahtarın değeri; boş Tree hata durumudur. |
| `LastKey() -> K` | En büyük anahtar; boş Tree hata durumudur. |
| `LastValue() -> V` | En büyük anahtarın değeri; boş Tree hata durumudur. |
| `FloorKeyOr(key: K, fallback: K) -> K` | Verilen anahtardan küçük/eşit en büyük anahtar; yoksa yedek. |
| `CeilKeyOr(key: K, fallback: K) -> K` | Verilen anahtardan büyük/eşit en küçük anahtar; yoksa yedek. |

<a id="enum"></a>

## enum

Enum değeri üzerinde çağrılır; mut gerekli değildir.

| İmza | Açıklama |
| --- | --- |
| `Name() -> string` | Enum değerinin adı. |
| `Value() -> U` | Enum'un temel integer tipindeki değeri. |
| `IsValid() -> bool` | Tanımlı enum değerlerinden biri mi? |

`Value` her zaman `i32` dönmez; enum'un declared underlying tipi esas alınır.
Enum tipi üzerinden kullanılan reflection API'leri bu instance methodlarının
yerine geçmez; ayrı std reflection yüzeyidir.

<a id="flagset"></a>

## flagset

Flagset değeri üzerinde çağrılır. Hepsi mut gerektirmeyen işlemlerdir.

| İmza | Açıklama |
| --- | --- |
| `Name() -> string` | Flagset'in ad gösterimi. |
| `Has(flags: F) -> bool` | Verilen bitlerin tamamı mevcut mu? |
| `HasAny(flags: F) -> bool` | Verilen bitlerden en az biri mevcut mu? |
| `With(flags: F) -> F` | Bitleri eklenmiş yeni değer. |
| `Without(flags: F) -> F` | Bitleri kaldırılmış yeni değer. |
| `Toggle(flags: F) -> F` | Verilen bitleri ters çevrilmiş yeni değer. |
| `Clear() -> F` | Tüm bitleri temizlenmiş yeni değer. |

Önemli: flagset `Clear()` **alıcıyı değiştirmez**; array/string/dictionary
`Clear()` gibi `void` dönen mutating bir method değildir. Alıcıyı değiştirmek
isteniyorsa dönen değer yeniden atanmalıdır. Enum'daki `Value()`/`IsValid()`
bu flagset intrinsic listesinde kayıtlı değildir.

<a id="async"></a>

## Async: coroutine<T> ve coroutine<void>

Timeout argümanlarının birimi **milisaniye**dir. Aşağıdaki methodlar `mut` alıcı
gerektirmez; `Start`/`Cancel` gibi işlemler paylaşılan task durumunu etkileyebilir.
Bu nedenle `mut` gerektirmemeleri işlemlerin saf olduğu anlamına gelmez.

| İmza: coroutine<T> | Açıklama |
| --- | --- |
| `Start() -> coroutine<T>` | Task'ı başlatır; task handle'ını döndürür. |
| `Cancel() -> void` | İptal isteği verir; thread'i zorla sonlandırmaz. |
| `IsReady() -> bool` | Task tamamlanmış mı? |
| `IsCancelled() -> bool` | İptal durumu. |
| `IsFaulted() -> bool` | Hata ile tamamlanmış mı? |
| `WaitFor(milliseconds: u64) -> bool` | Çağıran thread'de süreli bekleme; hazırsa `true`. |
| `Block() -> T` | Sonuç için çağıran thread'i bloklayarak bekler. |
| `Poll() -> std::async::TaskPoll<T>` | Beklemeden durum, optional sonuç ve hata bilgisini alır. |
| `Within(milliseconds: u64) -> coroutine<std::Option<T>>` | Başarıda `Some`, timeout'ta `None` üreten async işlem. |
| `CancelAfter(milliseconds: u64) -> coroutine<void>` | Süre sonunda task hâlâ bitmemişse iptal isteyen async işlem. |
| `Detach() -> void` | Task'ı detached çalışmaya bırakır. |

`coroutine<void>` üzerinde `Start`, `Block`, `Poll` ve `Within` dönüşleri şöyledir;
diğer methodların parametre ve dönüş tipleri üsttekiyle aynıdır:

| İmza: coroutine<void> | Açıklama |
| --- | --- |
| `Start() -> coroutine<void>` | Void task'ı başlatır. |
| `Block() -> void` | Tamamlanmayı bloklayarak bekler; değer üretmez. |
| `Poll() -> std::async::VoidTaskPoll` | Sonuç değeri olmadan durum ve hata bilgisi. |
| `Within(milliseconds: u64) -> coroutine<bool>` | Süre içinde tamamlandıysa `true`, timeout'ta `false`. |

`Poll` ve `Within` için ilgili `std::async` deklarasyonları, değerli `Within`
için ayrıca `std::Option` gerekir. `Within` timeout'ta kaynak task için iptal
isteği verir; hata/iptal, başarılı bir `None` sonucu ile aynı şey değildir.
`Within`/`CancelAfter` task döndürür; bunları senkron `WaitFor` ile karıştırma.
Dönen task normal async başlatma/await kurallarına tabidir; methodu çağırmak tek
başına her durumda zamanlayıcının çalışmaya başladığını varsaydırmaz.

<a id="pack"></a>

## Generic/value pack üyeleri

Bu yüzey normal array methodlarıyla aynı değildir. Pack'lerde iki **property**
ve bir method özel olarak tanınır:

| Üye | Parametre / sonuç | Koşul |
| --- | --- | --- |
| `size` | Argüman yok; `usize`. | Generic/value/type pack ve pack storage üzerinde. |
| `array` | Argüman yok; value-pack view veya type-pack view. | Değer pack'inde value view, tip pack'inde type view. |
| `ToStaticArray<T>() -> [T; N]` | Bir explicit generic eleman tipi; runtime argümanı yok. | Değer pack/view/storage için; `N` pack eleman sayısıdır. |

`args.size`, `args.array` biçiminde kullanılır; `size()`/`array()` değildir.
`array`, heterojen pack view olabilir, otomatik olarak `T[]` demek değildir.
`ToStaticArray<T>()` için tam bir generic tip argümanı gerekir; type pack'ini
runtime array'e dönüştürmek desteklenmez. Değerlerin hedef elemana uygunluğu
ayrıca korunmalıdır.

<a id="örnekler"></a>

## Kısa örnekler

### string: kopya ile yerinde değiştirme

```wio
mut title: string = "  Wio  ";
let trimmed: string = title.Trim(); // title değişmez
title.TrimInPlace();                // title değişir
title.Append(" language");

let number: i32 = "42".ToI32();
let hex: u32 = "ff".ToU32(16);
let maybeByte: std::Option<char> = title.Get(999usize);
let fallback: char = title.GetOr(999usize, '?');
```

### Dinamik ve sabit array

```wio
mut values: i32[] = [3, 1, 2];
values.Push(4);
values.Sort();
let first: std::Option<i32> = values.Get(0usize);
let copied: i32[] = values.Slice(1usize, 2usize);

let fixed: [i32; 3] = [3, 1, 2];
let sorted: [i32; 3] = fixed.Sorted();
let dynamic: i32[] = fixed.Take(2usize);
// fixed.Push(4); // sabit array boyutu değiştirilemez
```

### Dictionary: güvenli ve doğrudan erişim

```wio
mut scores: Dict<string, i32>;
scores.Set("Ada", 42);
let maybeScore: std::Option<i32> = scores.Get("missing");
let knownScore: i32 = scores.At("Ada");
let fallbackScore: i32 = scores.GetOr("missing", 0);

mut output: i32 = 0;
let found: bool = scores.TryGet("Ada", ref output);
```

### Unicode: byte, kod noktası, grapheme

```wio
let label: text = u"Merhaba 🌍";
let points: usize = label.Count();
let bytes: usize = label.ByteCount();
let globe: text = label.Slice(8usize, 1usize);
let codes: u32[] = label.CodePoints();
let pieces: text[] = label.Graphemes();
let utf8: string = label.Utf8();
```

## Kaynaklar ve bakım

Bu liste std kaynaklarından tahmin edilerek değil, şu kaynaklar birlikte
okunarak hazırlanmıştır. Özellikle resolver'ın ilk verdiği `Get`/`Poll`/`Within`
tipleri tek başına nihai kullanıcı imzası değildir; analyzer bunları tamamlar.

- [Intrinsic member resolver](../compiler/src/wio_compiler/sema/intrinsic_member_resolver.cpp):
  method adları, overload parametreleri, dönüş tipleri ve mut koşulları.
- [Access/call analizi](../compiler/src/wio_compiler/sema/detail/access_and_call_visitors.inl):
  `Option` dönüşleri, task sonuçları, pack üyeleri ve erişim kontrolleri.
- [Intrinsic kimlikleri](../compiler/include/wio/ast/ast.h): intrinsic envanteri.
- [Runtime intrinsic yardımcıları](../runtime/include/intrinsics.h): kesit,
  arama, mutation ve parse davranışları.
- [Unicode text runtime](../runtime/include/text.h): byte/kod noktası/grapheme ayrımı.
- [Async std yüzeyi](../std/async.wio) ve
  [async runtime](../runtime/include/std_async.h): poll, timeout, iptal ve bekleme.
- [Dönüşüm std yüzeyi](../std/convert.wio): `std::ToString<T>(value)` dahil
  kaynakta tanımlı dönüşümler. Her tipe gömülü `.ToString()` varmış gibi
  düşünülmemelidir.

Yeni intrinsic veya overload eklenince ilgili tablo da güncellenmelidir.
