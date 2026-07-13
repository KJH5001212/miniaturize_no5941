/*
 * flog.h — 플래시 오프라인 백로그 (웨어러블: 폰 없이 측정, 나중에 동기화)
 * ------------------------------------------------------------
 *  역할: BLE 미연결 동안 메인루프가 RAM 링(databuf)의 샘플을 내부 플래시
 *  파티션(flog_partition, 96KB)으로 옮겨 보관한다. 재연결하면 메인루프가
 *  이 백로그를 오래된 것부터 기존 {"d":...}+ack 스트림으로 재생한다
 *  (앱 무수정 — seq 재시작은 앱 파서의 "run restart" 경로가 흡수).
 *
 *  구조: 4KB 페이지 링. 페이지 = 헤더 16B + 레코드 255개×16B.
 *   - 레코드 = pstat_sample 재사용, _pad 에 CRC8+마커 → 찢어진 쓰기 검출
 *   - 페이지 헤더에 단조증가 page_no → 재부팅 후 tail/head 복원
 *   - 가득 차면 가장 오래된 페이지를 덮어씀 (flog_gap 표시)
 *   - 완전히 ack 된 페이지는 즉시 지움 (재부팅 후 중복 재전송 최소화)
 *
 *  스레딩: 모든 함수는 메인루프 전용 (측정 스레드는 databuf 만 만짐).
 *  용량: 24페이지 × 255 = 6,120샘플 ≈ cycle(5s@10Hz/5min) 기준 ~10시간.
 */
#ifndef FLOG_H_
#define FLOG_H_

#include "pstat.h"

int      flog_init(void);       /* 파티션 스캔, tail/head 복원. <0 = 사용불가 */
int      flog_push(const struct pstat_sample *s);
uint32_t flog_pending(void);    /* 미동기화 레코드 수 */
/* tail 부터 max 개 복사 (tail 전진 안함). 반환 = 개수 */
uint32_t flog_peek_batch(struct pstat_sample *out, uint32_t max);
void     flog_free_batch(uint32_t n);   /* ack 확인분 해제 + 완료 페이지 지움 */
bool     flog_gap(void);        /* 덮어쓰기 손실 발생 여부 */

#endif /* FLOG_H_ */
