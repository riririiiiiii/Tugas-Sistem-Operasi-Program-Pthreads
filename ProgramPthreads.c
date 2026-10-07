#include <stdio.h>      /* printf, fopen, fgets, fclose        */
#include <stdlib.h>     /* malloc, free, exit                  */
#include <pthread.h>    /* pthread_create, pthread_join, mutex */
 
/* ------------------------------------------------------------
 * VARIABEL GLOBAL (dipakai bersama oleh banyak thread)
 * ------------------------------------------------------------ */
 
/* Mutex untuk melindungi printf dan variabel tugas_selesai.
 * printf dikunci agar tulisan dari beberapa thread tidak bercampur. */
pthread_mutex_t lock_output = PTHREAD_MUTEX_INITIALIZER;
 
/* Menghitung berapa thread BAGIAN A yang sudah selesai.
 * Diubah oleh 3 thread sekaligus -> perlu dilindungi mutex. */
int tugas_selesai = 0;
 
/* Dua counter untuk BAGIAN B.
 * 'volatile' agar compiler tidak mengoptimasi increment-nya,
 * sehingga race condition benar-benar terlihat. */
volatile long counter_tanpa_lock = 0;
volatile long counter_dengan_lock = 0;
pthread_mutex_t lock_counter = PTHREAD_MUTEX_INITIALIZER;
 
#define JUMLAH_TAMBAH 1000000   /* tiap thread menambah 1.000.000 kali */
 
/* Struktur untuk mengirim parameter ke thread.
 * pthread_create hanya menerima SATU argumen (void *), jadi kalau
 * butuh banyak data kita bungkus dalam struct. */
typedef struct {
    int n;                 /* batas hitung (untuk faktorial / fibonacci) */
    const char *nama_file; /* nama file (untuk thread pembaca file)      */
} ParamThread;
 
/* ============================================================
 *  BAGIAN A - TIGA THREAD DENGAN PEKERJAAN BERBEDA
 * ============================================================ */
 
/* ---------- THREAD 1 : FAKTORIAL ----------
 * n! = 1 x 2 x 3 x ... x n
 * Contoh: 5! = 120
 * Tipe unsigned long long dipakai agar muat sampai 20!. */
void *hitung_faktorial(void *arg) {
    ParamThread *p = (ParamThread *)arg;   /* ubah void* ke tipe asli */
    unsigned long long hasil = 1;
 
    /* Proses hitung dilakukan DI LUAR lock, karena variabel 'hasil'
     * bersifat lokal (milik thread ini saja) -> aman, tidak perlu lock. */
    for (int i = 2; i <= p->n; i++) {
        hasil *= i;
    }
 
    /* ---- CRITICAL SECTION: printf & tugas_selesai dipakai bersama ---- */
    pthread_mutex_lock(&lock_output);
    printf("[Thread 1] Faktorial dari %d adalah %llu\n", p->n, hasil);
    tugas_selesai++;
    pthread_mutex_unlock(&lock_output);
    /* ---- END CRITICAL SECTION ---- */
 
    return NULL;
}
 
/* ---------- THREAD 2 : DERET FIBONACCI ----------
 * Deret: 0, 1, 1, 2, 3, 5, 8, 13, ...
 * Setiap suku = jumlah dua suku sebelumnya. */
void *tampil_fibonacci(void *arg) {
    ParamThread *p = (ParamThread *)arg;
    unsigned long long a = 0, b = 1;   /* dua suku awal */
 
    pthread_mutex_lock(&lock_output);
    printf("[Thread 2] Deret Fibonacci (%d suku): ", p->n);
    for (int i = 0; i < p->n; i++) {
        printf("%llu ", a);
        unsigned long long berikut = a + b;  /* hitung suku berikutnya */
        a = b;                               /* geser a */
        b = berikut;                         /* geser b */
    }
    printf("\n");
    tugas_selesai++;
    pthread_mutex_unlock(&lock_output);
 
    return NULL;
}
 
/* ---------- THREAD 3 : MEMBACA FILE TEKS ---------- */
void *baca_file(void *arg) {
    ParamThread *p = (ParamThread *)arg;
    char baris[256];
    int nomor = 0;
 
    FILE *f = fopen(p->nama_file, "r");   /* buka file mode baca */
 
    pthread_mutex_lock(&lock_output);
    if (f == NULL) {
        /* File tidak ditemukan / tidak bisa dibuka */
        printf("[Thread 3] ERROR: gagal membuka file '%s'\n", p->nama_file);
    } else {
        printf("[Thread 3] Isi file '%s':\n", p->nama_file);
        while (fgets(baris, sizeof(baris), f) != NULL) {
            nomor++;
            printf("    %d: %s", nomor, baris);
        }
        fclose(f);                         /* tutup file setelah selesai */
        printf("[Thread 3] Total %d baris dibaca.\n", nomor);
    }
    tugas_selesai++;
    pthread_mutex_unlock(&lock_output);
 
    return NULL;
}
 
/* ============================================================
 *  BAGIAN B - DEMO RACE CONDITION
 * ============================================================
 * Operasi counter++ sebenarnya 3 langkah:
 *    1. BACA nilai counter dari memori ke register
 *    2. TAMBAH 1 di register
 *    3. TULIS hasilnya kembali ke memori
 * Jika 2 thread menjalankan langkah-langkah ini bersamaan,
 * salah satu penambahan bisa HILANG (lost update).
 * ============================================================ */
 
/* TANPA mutex -> hasil akhir tidak dapat diprediksi (race condition) */
void *tambah_tanpa_lock(void *arg) {
    (void)arg;
    for (int i = 0; i < JUMLAH_TAMBAH; i++) {
        counter_tanpa_lock++;              /* TIDAK aman */
    }
    return NULL;
}
 
/* DENGAN mutex -> hanya 1 thread yang boleh menambah pada satu waktu */
void *tambah_dengan_lock(void *arg) {
    (void)arg;
    for (int i = 0; i < JUMLAH_TAMBAH; i++) {
        pthread_mutex_lock(&lock_counter);
        counter_dengan_lock++;             /* aman */
        pthread_mutex_unlock(&lock_counter);
    }
    return NULL;
}
 
/* ============================================================
 *  FUNGSI UTAMA
 * ============================================================ */
int main(void) {
    pthread_t t1, t2, t3;
 
    /* ---------------- BAGIAN A ---------------- */
    printf("=== BAGIAN A: 3 Thread Pekerjaan Berbeda ===\n");
 
    ParamThread p_fak = { 10, NULL };        /* faktorial 10 */
    ParamThread p_fib = { 10, NULL };        /* fibonacci 10 suku */
    ParamThread p_file = { 0, "data.txt" };  /* baca data.txt */
 
    /* Membuat (menjalankan) tiga thread.
     * Parameter: (id thread, atribut, fungsi thread, argumen fungsi) */
    if (pthread_create(&t1, NULL, hitung_faktorial, &p_fak) != 0 ||
        pthread_create(&t2, NULL, tampil_fibonacci, &p_fib) != 0 ||
        pthread_create(&t3, NULL, baca_file, &p_file) != 0) {
        fprintf(stderr, "Gagal membuat thread\n");
        return 1;
    }
 
    /* Tunggu ketiga thread selesai sebelum lanjut. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    printf("Semua thread selesai. Total tugas selesai = %d (harus 3)\n\n",
           tugas_selesai);
 
    /* ---------------- BAGIAN B ---------------- */
    printf("=== BAGIAN B: Demo Race Condition ===\n");
    printf("Tiga thread, masing-masing menambah counter %d kali.\n", JUMLAH_TAMBAH);
    printf("Hasil yang BENAR = %d\n\n", 3 * JUMLAH_TAMBAH);
 
    pthread_t w[3];
 
    /* B1: TANPA mutex */
    for (int i = 0; i < 3; i++) pthread_create(&w[i], NULL, tambah_tanpa_lock, NULL);
    for (int i = 0; i < 3; i++) pthread_join(w[i], NULL);
    printf("TANPA mutex : counter = %ld  <- sering salah (race condition)\n",
           counter_tanpa_lock);
 
    /* B2: DENGAN mutex */
    for (int i = 0; i < 3; i++) pthread_create(&w[i], NULL, tambah_dengan_lock, NULL);
    for (int i = 0; i < 3; i++) pthread_join(w[i], NULL);
    printf("DENGAN mutex: counter = %ld  <- selalu benar\n",
           counter_dengan_lock);
 
    /* Bersihkan mutex */
    pthread_mutex_destroy(&lock_output);
    pthread_mutex_destroy(&lock_counter);
    return 0;
}