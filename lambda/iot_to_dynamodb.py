import json
import urllib.request
import uuid
import time

# AppSync 환경 설정 (amplify_outputs.json 참고)
APPSYNC_API_URL = "https://daubpxrxi5g3xff4hvp44gaoie.appsync-api.ap-northeast-2.amazonaws.com/graphql"
API_KEY = "da2-6cdrxj6z65dgzjqtt2zeg3uvau"

def lambda_handler(event, context):
    try:
        device_id = event.get('device_id')
        
        # ⚠️ 테스트의 편의를 위해, 그리고 실시간 차트 확인을 위해 
        # 테스트 이벤트의 과거 시간 대신 Lambda가 실행되는 '현재 시간'을 강제로 주입합니다.
        current_timestamp = int(time.time())
        
        if not device_id:
            raise ValueError("Missing device_id")
            
        # AppSync GraphQL Mutation 작성
        query = """
        mutation CreateEfficiencyData($input: CreateEfficiencyDataInput!) {
            createEfficiencyData(input: $input) {
                id
                timestamp
                efficiency
            }
        }
        """
        
        variables = {
            "input": {
                "id": str(uuid.uuid4()),
                "device_id": str(device_id),
                "timestamp": current_timestamp,
                "v_in": float(event.get('v_in', 0)),
                "i_in": float(event.get('i_in', 0)),
                "p_in": float(event.get('p_in', 0)),
                "v_out": float(event.get('v_out', 0)),
                "i_out": float(event.get('i_out', 0)),
                "p_out": float(event.get('p_out', 0)),
                "efficiency": float(event.get('efficiency', 0)),
                "status": str(event.get('status', 'Normal'))
            }
        }
        
        # HTTP POST 요청으로 AppSync API 호출
        req = urllib.request.Request(APPSYNC_API_URL, method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("x-api-key", API_KEY)
        
        payload = json.dumps({"query": query, "variables": variables}).encode("utf-8")
        
        with urllib.request.urlopen(req, data=payload) as response:
            result = json.loads(response.read().decode())
            print("AppSync Response:", result)
            
            # AppSync에서 에러를 반환했는지 체크
            if 'errors' in result:
                raise Exception(str(result['errors']))
            
        return {
            "statusCode": 200,
            "body": json.dumps("Data successfully sent to AppSync!")
        }
        
    except Exception as e:
        print(f"Error processing IoT payload: {e}")
        return {
            "statusCode": 500,
            "body": json.dumps(f"Error: {str(e)}")
        }
