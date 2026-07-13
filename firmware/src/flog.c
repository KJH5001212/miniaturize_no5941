/*
 * flog.c — 플래시 오프라인 백로그 구현 (설계는 flog.h 참조)
 *
 *  포지션 체계: tail_pos/head_pos 는 단조증가 레코드 카운터.
 *   페이지번호 page_no = pos / REC_PER_PG, 물리 페이지 = page_no % N_PAGES.
 *  nRF52 내부 플래시: 4KB 페이지, 4바이트 워드 쓰기, 지운 상태 = 0xFF.
 *  BLE 공존은 Zephyr 플래시 드라이버의 radio-sync 가 처리 (MPSL 타임슬롯).
 */
#include "flog.h"

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(flog, LOG_LEVEL_INF);

#define PAGE_SZ    4096U
#define REC_SZ     16U                       /* sizeof(pstat_sample) 고정 확인 */
#define HDR_SZ     16U
#define REC_PER_PG ((PAGE_SZ - HDR_SZ) / REC_SZ)   /* 255 */

#define HDR_MAGIC  0x50464C31U               /* "1LFP" (LE) */
#define REC_MARK0  0x5AU
#define REC_MARK1  0xA5U

struct page_hdr {
	uint32_t magic;
	uint32_t page_no;    /* 단조증가 (물리 = page_no % N_PAGES) */
	uint32_t rsv0;
	uint32_t rsv1;
};

BUILD_ASSERT(sizeof(struct pstat_sample) == REC_SZ, "record size");
BUILD_ASSERT(sizeof(struct page_hdr) == HDR_SZ, "header size");

static const struct flash_area *fa;
static uint32_t n_pages;
static uint32_t tail_pos, head_pos;   /* 레코드 단위 절대 카운터 */
static bool     had_gap;
static bool     usable;

/* CRC-8 (poly 0x31), seq..range 13바이트 대상 */
static uint8_t crc8(const uint8_t *p, size_t n)
{
	uint8_t c = 0xFF;
	while (n--) {
		c ^= *p++;
		for (int i = 0; i < 8; i++) {
			c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
		}
	}
	return c;
}

static uint32_t pg_of(uint32_t pos)   { return pos / REC_PER_PG; }
static uint32_t phys_of(uint32_t pgno){ return pgno % n_pages; }
static uint32_t rec_off(uint32_t pos)
{
	return phys_of(pg_of(pos)) * PAGE_SZ + HDR_SZ + (pos % REC_PER_PG) * REC_SZ;
}

static bool rec_valid(const struct pstat_sample *r)
{
	return r->_pad[1] == REC_MARK0 && r->_pad[2] == REC_MARK1 &&
	       r->_pad[0] == crc8((const uint8_t *)r, 13);
}

static int erase_phys(uint32_t phys)
{
	return flash_area_erase(fa, phys * PAGE_SZ, PAGE_SZ);
}

int flog_init(void)
{
	if (flash_area_open(FIXED_PARTITION_ID(flog_partition), &fa) != 0) {
		LOG_ERR("flog partition open 실패 — 백로그 비활성");
		return -1;
	}
	n_pages = fa->fa_size / PAGE_SZ;

	/* 스캔: 유효 헤더의 min/max page_no 로 tail/head 페이지 복원 */
	bool any = false;
	uint32_t min_no = 0, max_no = 0;
	for (uint32_t p = 0; p < n_pages; p++) {
		struct page_hdr h;
		if (flash_area_read(fa, p * PAGE_SZ, &h, sizeof(h)) != 0) {
			continue;
		}
		if (h.magic != HDR_MAGIC || phys_of(h.page_no) != p) {
			continue;
		}
		if (!any || (int32_t)(h.page_no - max_no) > 0) max_no = h.page_no;
		if (!any || (int32_t)(min_no - h.page_no) > 0) min_no = h.page_no;
		any = true;
	}
	if (!any) {
		tail_pos = head_pos = 0;
		usable = true;
		LOG_INF("flog: 비어있음 (%u pages, %u rec/pg)", n_pages, REC_PER_PG);
		return 0;
	}
	tail_pos = min_no * REC_PER_PG;
	/* head: 최신 페이지 안에서 유효 레코드 수 세기 (write 는 순차라 앞에서부터) */
	head_pos = max_no * REC_PER_PG;
	for (uint32_t i = 0; i < REC_PER_PG; i++) {
		struct pstat_sample r;
		if (flash_area_read(fa, rec_off(head_pos), &r, REC_SZ) != 0 ||
		    !rec_valid(&r)) {
			break;
		}
		head_pos++;
	}
	usable = true;
	LOG_INF("flog: 백로그 %u개 복원 (tail=%u head=%u)",
		head_pos - tail_pos, tail_pos, head_pos);
	return 0;
}

int flog_push(const struct pstat_sample *s)
{
	if (!usable) {
		return -ENODEV;
	}
	uint32_t pgno = pg_of(head_pos);

	if ((head_pos % REC_PER_PG) == 0) {
		/* 새 페이지 진입: 링 가득이면 가장 오래된 페이지 희생 */
		if (pgno >= n_pages && pg_of(tail_pos) <= pgno - n_pages) {
			tail_pos = (pgno - n_pages + 1) * REC_PER_PG;
			had_gap = true;
		}
		if (erase_phys(phys_of(pgno)) != 0) {
			return -EIO;
		}
		struct page_hdr h = { .magic = HDR_MAGIC, .page_no = pgno,
				      .rsv0 = 0xFFFFFFFF, .rsv1 = 0xFFFFFFFF };
		if (flash_area_write(fa, phys_of(pgno) * PAGE_SZ, &h, sizeof(h)) != 0) {
			return -EIO;
		}
	}

	struct pstat_sample r = *s;
	r._pad[0] = crc8((const uint8_t *)&r, 13);
	r._pad[1] = REC_MARK0;
	r._pad[2] = REC_MARK1;
	if (flash_area_write(fa, rec_off(head_pos), &r, REC_SZ) != 0) {
		return -EIO;
	}
	head_pos++;
	return 0;
}

uint32_t flog_pending(void)
{
	return usable ? head_pos - tail_pos : 0;
}

uint32_t flog_peek_batch(struct pstat_sample *out, uint32_t max)
{
	if (!usable) {
		return 0;
	}
	uint32_t k = 0;
	uint32_t pos = tail_pos;
	while (pos < head_pos && k < max) {
		if (flash_area_read(fa, rec_off(pos), &out[k], REC_SZ) != 0 ||
		    !rec_valid(&out[k])) {
			break;   /* 손상 레코드 → 그 앞까지만 */
		}
		k++;
		pos++;
	}
	return k;
}

void flog_free_batch(uint32_t n)
{
	if (!usable) {
		return;
	}
	uint32_t old_pg = pg_of(tail_pos);
	tail_pos += n;
	if (tail_pos > head_pos) {
		tail_pos = head_pos;
	}
	/* 완전히 소비된 페이지는 즉시 지움 → 재부팅 후 중복 재전송 방지 */
	for (uint32_t pg = old_pg; pg < pg_of(tail_pos); pg++) {
		erase_phys(phys_of(pg));
	}
	if (tail_pos == head_pos && (head_pos % REC_PER_PG) != 0) {
		/* 백로그 완전 소진 — 부분 페이지도 정리하고 카운터 리셋 */
		erase_phys(phys_of(pg_of(head_pos)));
		tail_pos = head_pos = pg_of(head_pos) * REC_PER_PG;
	}
}

bool flog_gap(void)
{
	return had_gap;
}
