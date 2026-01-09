#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <string_view> // stringview için

// --- HOCANIN VERDİĞİ YAPILAR (STRUCTS) ---
// bunları değiştirmedim, aynen kullanıyorum
struct ZoneCount {
    std::string zone;
    long long count;
};

struct SlotCount {
    std::string zone;
    int hour;
    long long count;
};

// --- BENİM PERFORMANS İÇİN YAZDIĞIM YAPI ---
// map içinde her seferinde yeni obje oluşturmamak için bunu kurdum
struct ZoneData {
    std::string name;
    long long total = 0;
    long long hourly[24] = { 0 };
};

class TripAnalyzer {
private:
    // --- VERİ SAKLAMA ALANI ---
    // tüm datayı bu hash map içinde tutuyorum, erişim hızlı olsun diye
    std::unordered_map<std::string, ZoneData> z_map;

    // --- MANUEL SAAT AYIKLAMA FONKSİYONU ---
    // stoi fonksiyonu çok yavaş çalıştığı için string içinden saati elle çektim
    // parametreyi string_view yaptım.
    int get_hour_manual(std::string_view s);

    // --- SIRALAMA KARŞILAŞTIRICILARI (COMPARATORS) ---
    // sort fonksiyonuna parametre olarak gönderilecekler
    static bool compare_zones(const ZoneCount& a, const ZoneCount& b);
    static bool compare_slots(const SlotCount& a, const SlotCount& b);

public:
    // --- DOSYA OKUMA VE İŞLEME ---
    void ingestFile(const std::string& filePath);

    // --- RAPORLAMA: EN ÇOK GİDİLEN YERLER ---
    std::vector<ZoneCount> topZones(int n) const;

    // --- RAPORLAMA: EN YOĞUN SAATLER ---
    std::vector<SlotCount> topBusySlots(int n) const;
};

#endif
