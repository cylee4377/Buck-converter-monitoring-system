import json
import boto3
import uuid
import time
from decimal import Decimal

# Initialize DynamoDB resource
dynamodb = boto3.resource('dynamodb')

# DynamoDB Table Name (Amplify Gen2 자동 생성 테이블)
TABLE_NAME = 'EfficiencyData-hpwltwq24jddvmfgzndquauis4-NONE'
table = dynamodb.Table(TABLE_NAME)

def lambda_handler(event, context):
    """
    AWS IoT Core에서 전달된 벅 컨버터 데이터를 DynamoDB에 저장하는 Lambda 함수
    event: IoT Core Rule을 통해 전달받은 JSON payload (Data_Spec.md 규격 준수)
    """
    try:
        # 필수 필드 검증
        device_id = event.get('device_id')
        timestamp = event.get('timestamp')
        
        if not device_id or not timestamp:
            raise ValueError("Missing required fields: device_id or timestamp")
            
        # Data_Spec.md Rule 3: DynamoDB TTL 적용 (비용 관리, 7일 보존)
        ttl_value = int(time.time()) + (7 * 24 * 60 * 60)
        
        # DynamoDB는 Float 타입을 직접 지원하지 않으므로 Decimal로 변환 필요
        # 부동소수점 오차를 방지하기 위해 str() 변환 후 Decimal() 적용
        item = {
            'id': str(uuid.uuid4()),          # PK: 고유 UUID 생성
            'timestamp': int(timestamp),        # SK: 측정 시간
            'device_id': str(device_id),        # GSI PK (byDevice)
            'v_in': Decimal(str(event.get('v_in', 0))),
            'i_in': Decimal(str(event.get('i_in', 0))),
            'p_in': Decimal(str(event.get('p_in', 0))),
            'v_out': Decimal(str(event.get('v_out', 0))),
            'i_out': Decimal(str(event.get('i_out', 0))),
            'p_out': Decimal(str(event.get('p_out', 0))),
            'efficiency': Decimal(str(event.get('efficiency', 0))),
            'ttl': ttl_value                    # TTL 속성
        }
        
        # 상태 정보 (status)는 옵션이므로 존재하는 경우에만 추가
        if 'status' in event:
            item['status'] = str(event['status'])
            
        # DynamoDB 테이블에 데이터 삽입
        response = table.put_item(Item=item)
        
        print(f"Successfully inserted item for device {device_id} at {timestamp}")
        
        return {
            'statusCode': 200,
            'body': json.dumps('Data successfully written to DynamoDB!')
        }
        
    except Exception as e:
        print(f"Error processing IoT payload: {e}")
        print(f"Event payload: {json.dumps(event)}")
        return {
            'statusCode': 500,
            'body': json.dumps(f'Error processing data: {str(e)}')
        }
