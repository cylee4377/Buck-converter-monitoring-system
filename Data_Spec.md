# 프로젝트 데이터 아키텍처 가이드 (Data Schema Guideline)

> **상태:** 확정 (v1.1)
> **대상:** Buck Converter Efficiency Real-Time Measurement System
> **핵심 원칙:** AWS Amplify GraphQL 스키마를 기준으로 모든 데이터 통신 규격을 통일한다.

---

## ⚠️ 코드 생성 규칙 (모든 파트 공통 적용)

> 아래의 필드명과 타입은 **절대 변경 불가**. ESP32 C++, Lambda Python, React JS 등 모든 코드에서 동일하게 사용한다.
>
> - 필드명 예시: `v_in` ✅ | `vin` ❌ | `V_in` ❌ | `voltage_in` ❌
> - 타입 예시: `Float` (소수점 포함) ✅ | `int` ❌

---

## 1. AWS Amplify (GraphQL) 스키마 정의

본 프로젝트의 모든 백엔드 인프라와 API 인터페이스는 아래의 `schema.graphql`을 기반으로 구축한다.

```graphql
type EfficiencyData @model
@auth(rules: [{ allow: public }]) {
  id: ID!                    # 고유 식별값 (UUID, Amplify 자동 생성)
  device_id: String! @index(name: "byDevice", sortKeyFields: ["timestamp"])
                             # 기기 식별용 (예: "ST_UNIT_01")
  timestamp: AWSTimestamp!   # 측정 시간 (Unix Epoch, 초 단위)

  # --- 입력단 데이터 ---
  v_in: Float!               # 입력 전압 [V]
  i_in: Float!               # 입력 전류 [A]
  p_in: Float!               # 입력 전력 [W] = v_in * i_in

  # --- 출력단 데이터 ---
  v_out: Float!              # 출력 전압 [V]
  i_out: Float!              # 출력 전류 [A]
  p_out: Float!              # 출력 전력 [W] = v_out * i_out

  # --- 계산된 효율 ---
  efficiency: Float!         # 변환 효율 [%] = (p_out / p_in) * 100

  # --- 상태 정보 (옵션) ---
  status: String             # "Normal" | "Overload" | "Offline"
}
```

---

## 2. 데이터 프로토콜 상세 (규격 엄수)

### 2.1 엣지 디바이스 (ESP32) MQTT 전송 포맷

ESP32에서 AWS IoT Core로 발행하는 MQTT 메시지는 아래의 JSON 형식을 반드시 준수해야 한다.

- **MQTT Topic:** `buck/efficiency/{device_id}` (예: `buck/efficiency/ST_UNIT_01`)
- **QoS:** 1 (At least once)
- **전송 주기:** 1초 ~ 5초 (실시간 변화 포착 최적화)

```json
{
  "device_id": "ST_UNIT_01",
  "v_in":       12.0,
  "i_in":        0.8,
  "p_in":        9.6,
  "v_out":       5.0,
  "i_out":       1.7,
  "p_out":       8.5,
  "efficiency": 88.54,
  "timestamp":  1715472000
}
```

> **주의:** `p_in`, `p_out`은 ESP32 펌웨어 내에서 계산 후 포함하여 전송한다.  
> Lambda는 검증 목적으로만 재계산하며, 저장값은 ESP32 전송값을 우선으로 한다.

### 2.2 DynamoDB 물리적 저장 구조

Amplify Gen2 배포 시 자동 생성되는 DynamoDB 테이블 속성은 다음과 같다.
실제 테이블명: `EfficiencyData-hpwltwq24jddvmfgzndquauis4-NONE`

| 속성명 | DynamoDB 타입 | 역할 | 비고 |
|--------|--------------|------|------|
| **id** | String (PK) | 고유 UUID | Amplify 자동 생성, 단일 PK |
| **device_id** | String | 기기 식별자 | GSI 미사용, 일반 속성 |
| **timestamp** | Number | 측정 시간 (Unix Epoch) | ~~SK~~ → 일반 속성 (Amplify Gen2 스키마 기준) |
| **v_in** | Number | 입력 전압 [V] | |
| **i_in** | Number | 입력 전류 [A] | |
| **p_in** | Number | 입력 전력 [W] | |
| **v_out** | Number | 출력 전압 [V] | |
| **i_out** | Number | 출력 전류 [A] | |
| **p_out** | Number | 출력 전력 [W] | |
| **efficiency** | Number | 최종 변환 효율 [%] | |
| **status** | String | 동작 상태 | Nullable |
| **ttl** | Number | TTL 만료 시각 (Unix Epoch) | 비용 관리용 |

> **[v1.1 → v1.2 변경 이유]** 초기 설계(`v1.1`)에서는 `timestamp`를 SK로 사용하는 수동 테이블을 상정했으나,
> AWS Amplify Gen2 배포 시 AppSync 스키마 기준으로 테이블이 자동 생성되며 SK 없이 `id`만 PK로 구성된다.
> `timestamp`는 SK에서 일반 속성으로 역할이 변경되었으나, **ESP32의 MQTT 전송 포맷 및 Lambda의 저장 로직은 변경 없음**.

---

## 3. 레이어별 필드명 대조표 (Cross-Layer Reference)

| 물리량 | ESP32 (C++) 변수명 | MQTT JSON 키 | GraphQL 필드 | DynamoDB 속성 |
|--------|-------------------|-------------|-------------|--------------|
| 입력 전압 | `float v_in` | `"v_in"` | `v_in` | `v_in` |
| 입력 전류 | `float i_in` | `"i_in"` | `i_in` | `i_in` |
| 입력 전력 | `float p_in` | `"p_in"` | `p_in` | `p_in` |
| 출력 전압 | `float v_out` | `"v_out"` | `v_out` | `v_out` |
| 출력 전류 | `float i_out` | `"i_out"` | `i_out` | `i_out` |
| 출력 전력 | `float p_out` | `"p_out"` | `p_out` | `p_out` |
| 효율 | `float efficiency` | `"efficiency"` | `efficiency` | `efficiency` |
| 기기 ID | `String device_id` | `"device_id"` | `device_id` | `device_id` |
| 타임스탬프 | `unsigned long timestamp` | `"timestamp"` | `timestamp` | `timestamp` |

---

## 4. 안티그래비티 코드 생성 수행 지침 (Action Items)

### 📌 Rule 1 — 필드명 고정 (Field Name Consistency)
모든 코드(ESP32 C++, Lambda Python, React JS)에서 **섹션 3의 대조표**를 기준으로 필드명과 데이터 타입을 통일한다. 임의 변경 시 데이터 파이프라인이 단절된다.

### 📌 Rule 2 — 실시간성 우선 (GraphQL Subscription First)
프론트엔드(React) 개발 시, DynamoDB 직접 폴링이 아닌 **GraphQL Subscription**을 우선 사용하여 실시간 그래프 업데이트를 구현한다.

```javascript
// ✅ 권장 방식
const subscription = API.graphql(
  graphqlOperation(onCreateEfficiencyData)
).subscribe({ next: ({ value }) => updateChart(value.data) });

// ❌ 비권장 방식 (폴링)
setInterval(() => fetchLatestData(), 2000);
```

### 📌 Rule 3 — DynamoDB TTL 적용 (Cost Management)
대량의 시계열 데이터 적재에 따른 비용 증가를 방지하기 위해, Lambda에서 저장 시 `ttl` 필드를 함께 기록한다.

```python
# Lambda (Python) TTL 예시 — 7일 보존
import time
ttl_value = int(time.time()) + (7 * 24 * 60 * 60)
item["ttl"] = ttl_value
```

DynamoDB 콘솔에서 TTL 속성명을 `ttl`로 설정하면 자동 만료된다.

---

## 5. 관련 문서

| 문서 | 경로 | 설명 |
|------|------|------|
| 설계 기준 문서 | `Buck_Converter_Efficiency_System.md` | 시스템 전체 아키텍처 및 하드웨어 구성 |
| 데이터 스키마 가이드 | `Data_Spec.md` (본 문서) | 데이터 규격 및 코드 생성 규칙 |

---

*본 규격은 확정(v1.1) 상태이며, 변경 시 모든 레이어에 동시 반영 필요.*
