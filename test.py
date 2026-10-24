import pandas as pd
import sys

def generate_mermaid(tx_log, rx_log):
    try:
        df_tx = pd.read_csv(tx_log)
        df_rx = pd.read_csv(rx_log)
    except Exception as e:
        print(f"Errore caricamento file: {e}")
        return

    merged = pd.concat([df_tx, df_rx]).sort_values('Timestamp_us')

    print("sequenceDiagram")
    print("    participant TX")
    print("    participant RX")
    
    for _, row in merged.iterrows():
        d = row['Direction']
        t = row['Type']
        pid = row['ID']
        seg = row['Segment']
        note = row['Notes'] if pd.notna(row['Notes']) else ""
        
        # Formattazione etichetta
        label = f"{t} [ID:{pid} S:{seg}] {note}".strip()

        if d == "TX":
            # TX invia verso RX
            if t in ["ACK", "FREE_ACK", "COMPLETE_MSG"]:
                print(f"    TX-->>RX: {label}")
            else:
                print(f"    TX->>RX: {label}")
                
        elif d == "RX":
            # RX invia verso TX
            if t in ["ACK", "FREE_ACK", "COMPLETE_MSG"]:
                print(f"    RX-->>TX: {label}")
            else:
                print(f"    RX->>TX: {label}")
                
        elif d == "NONE":
            # Eventi interni (Timeout, Reset, Errori)
            # Determiniamo su quale colonna scrivere la nota in base al tipo o al log di origine
            if t in ["EXPIRED", "RESEND_TIMEOUT"]:
                print(f"    Note over TX,RX: ⚠️ {label}")
            elif t == "RECEPTION_RESET":
                print(f"    Note left of RX: 🔄 {label}")
            else:
                print(f"    Note over TX,RX: {label}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Uso: python script.py tx_log.csv rx_log.csv")
    else:
        generate_mermaid(sys.argv[1], sys.argv[2])
