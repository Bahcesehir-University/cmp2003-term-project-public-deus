#include "analyzer.h"
#include <fstream>
#include <algorithm>
#include <string_view> // string view için 

// =============================================================
// BÖLÜM 1: YARDIMCI FONKSİYONLAR
// =============================================================

// --- BOŞLUK TEMİZLEME (TRIM) ---
// Verinin başında ve sonunda kalan gereksiz boşlukları burada sildim.
// Örneğin " ZONE01 " gelirse bunu "ZONE01" haline getirdim.
// "Dirty Data" testinden geçmek için bu kontrolü ekledim.
// string_view döndürecek şekilde hızlandırmaya çalıştım.
static std::string_view trim(std::string_view str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string_view::npos == first) {
        return {};
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// ============================================================
// BÖLÜM 2: VERİ AYIKLAMA (PARSING) MANTIĞI
// ============================================================

int TripAnalyzer::get_hour_manual(std::string_view s) {
    // 1. UZUNLUK KONTROLÜ: Tarih formatı çok kısaysa hatalıdır diye düşündüm
    // ve -1 döndürerek bu satırı iptal ettim.
    if (s.length() < 10) return -1;

    int space_index = -1;
    for (size_t i = 0; i < s.length(); i++) {
        //Tarih ve saat arasındaki ayırıcı olarak sadece BOŞLUK karakterini aradım.
        if (s[i] == ' ') {
            space_index = (int)i;
            break;
        }
    }

    // Eğer boşluk bulamadıysam formatın hatalı olduğuna karar verdim.
    if (space_index == -1 || space_index + 2 >= (int)s.length()) {
        return -1;
    }

    // Saati temsil eden karakterleri buradan çektim.
    char c1 = s[space_index + 1];
    char c2 = s[space_index + 2];

    // stoi fonksiyonu yavaş çalıştığı için burada manuel kontrol yaptım.
    // Karakterlerin gerçekten rakam olup olmadığına baktım.
    bool is_digit1 = (c1 >= '0' && c1 <= '9');
    bool is_digit2 = (c2 >= '0' && c2 <= '9');

    if (is_digit1) {
        if (is_digit2) {
            // İki haneli saat (Örn: 14:00) -> Karakterleri sayıya çevirip birleştirdim.
            return (c1 - '0') * 10 + (c2 - '0');
        }
        else if (c2 == ':' || c2 == ' ') {
            // Tek haneli saat (Örn: 9:00) -> Sadece ilk rakamı aldım.
            return c1 - '0';
        }
    }

    // Format uymadıysa -1 döndürdüm, böylece bu bozuk satırı atladım.
    return -1;
}

// =============================================================
// BÖLÜM 3: SIRALAMA ALGORİTMALARI (COMPARATORS)
// =============================================================

// En çok gidilen bölgeleri sıralamak için bu fonksiyonu yazdım.
bool TripAnalyzer::compare_zones(const ZoneCount& a, const ZoneCount& b) {
    // Önce sayıya göre büyükten küçüğe sıraladım.
    if (a.count != b.count) return a.count > b.count;
    // Eşitlik durumunda (Tie-Breaker) proje kuralına uyarak alfabetik sıralama yaptım.
    return a.zone < b.zone;
}

// En yoğun saatleri sıralamak için bu mantığı kurdum.
bool TripAnalyzer::compare_slots(const SlotCount& a, const SlotCount& b) {
    // Önce sayıya göre sıraladım.
    if (a.count != b.count) return a.count > b.count;
    // Eşitlik bozulmazsa Zone ID'ye göre sıraladım.
    if (a.zone != b.zone) return a.zone < b.zone;
    // Yine bozulmazsa saate göre küçükten büyüğe sıraladım.
    return a.hour < b.hour;
}

// =============================================================
// BÖLÜM 4: DOSYA OKUMA VE İŞLEME
// =============================================================

void TripAnalyzer::ingestFile(const std::string& filePath) {
    std::ios_base::sync_with_stdio(false);//Kodu c den ayırarak hızlandırdım.

    std::ifstream infile(filePath);
    if (!infile.is_open()) return; // Dosya açılmazsa fonksiyondan çıktım.

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue; // Boş satırları atladım.

        //string_view ile kopyalamasız işlem yaptım
        std::string_view row(line);

        // --- VİRGÜL BULMA (MANUEL PARSING) ---
        // stringstream kullanmak yerine, satır içindeki virgülleri tek tek saydım.
        // Bu yöntem milyonlarca satırda daha hızlı.
        int comma_pos[10];
        int c_idx = 0;

        for (size_t i = 0; i < row.length(); i++) {
            if (row[i] == ',') {
                if (c_idx < 10) {
                    comma_pos[c_idx] = (int)i;
                    c_idx++;
                }
            }
        }

        // --- SÜTUN SAYISI KONTROLÜ ---
        // 6 sütunlu bir yapı olduğu için en az 5 virgül şartı koydum.
        // Eksik sütun varsa bu satırı işlemeden geçtim (continue).
        if (c_idx < 5) continue;

        try {
            // İlgili sütunları virgülden virgüle substring ile kestim.
            // string_view kullandığım için artık O(1) hızında.

            // PickupZoneID (1. ve 2. virgül arası)
            std::string_view z_id_raw = row.substr(comma_pos[0] + 1, comma_pos[1] - comma_pos[0] - 1);

            // PickupDateTime (3. ve 4. virgül arası)
            std::string_view d_time_raw = row.substr(comma_pos[2] + 1, comma_pos[3] - comma_pos[2] - 1);

            // Verileri alırken yazdığım trim fonksiyonuyla temizledim.
            std::string_view z_id_view = trim(z_id_raw);
            std::string_view d_time_view = trim(d_time_raw);

            // Eğer ID boş geldiyse bu veriyi kaydetmedim.
            if (z_id_view.empty()) continue;

            // Saati yukarıda yazdığım fonksiyonla ayıkladım.
            int h = get_hour_manual(d_time_view);

            // Saat formatı hatalıysa veya mantıksızsa (25 gibi) kaydetmedim.
            if (h == -1) continue;
            if (h < 0 || h > 23) continue;

            // Veriyi Map yapısına kaydettim.
            // Referans (&) kullanarak gereksiz kopyalamadan kaçındım.
            // Map key için mecburen string oluşturuyoruz ama buraya kadar allocation yapmadık.
            std::string key(z_id_view);

            ZoneData& item = z_map[key];
            if (item.name.empty()) item.name = key;
            item.total++;       // Toplam sayacı arttırdım.
            item.hourly[h]++;   // O saatin sayacını arttırdım.
        }
        catch (...) {
            // Beklenmedik bir hata olursa program çökmesin diye continue ekledim.
            continue;
        }
    }
}

// =============================================================
// BÖLÜM 5: RAPORLAMA FONKSİYONLARI
// =============================================================

std::vector<ZoneCount> TripAnalyzer::topZones(int n) const {
    std::vector<ZoneCount> v1;
    // Performans için vektör boyutunu baştan ayırdım (reserve ettim).
    v1.reserve(z_map.size());

    // Map içindeki verileri vektöre aktardım.
    for (const auto& kv : z_map) {
        v1.push_back({ kv.second.name, kv.second.total });
    }

    // İstenen sayı (n) mevcut listeden büyükse hata vermemesi için kontrol ettim.
    if (n > (int)v1.size()) n = (int)v1.size();

    // Yazdığım compare_zones fonksiyonuna göre sıraladım.
    std::partial_sort(v1.begin(), v1.begin() + n, v1.end(), compare_zones);

    return std::vector<ZoneCount>(v1.begin(), v1.begin() + n);
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int n) const {
    std::vector<SlotCount> v2;
    // Bir gün en fazla 24 saat olabilir, garanti olsun diye baştan hepsine yer açtım.
    v2.reserve(z_map.size() * 24);

    // Her bölgenin saatlik verilerini taradım.
    for (const auto& kv : z_map) {
        for (int h = 0; h < 24; h++) {
            if (kv.second.hourly[h] > 0) {
                // Sadece verisi olan saatleri listeye ekledim.
                v2.push_back({ kv.second.name, h, kv.second.hourly[h] });
            }
        }
    }

    if (n > (int)v2.size()) n = (int)v2.size();

    // Yazdığım compare_slots fonksiyonuna göre sıraladım.
    std::partial_sort(v2.begin(), v2.begin() + n, v2.end(), compare_slots);

    return std::vector<SlotCount>(v2.begin(), v2.begin() + n);
}

