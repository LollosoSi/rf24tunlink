import pandas as pd
import sys
import base64

def copy_to_clipboard(text):
    """Copia negli appunti tramite sequenza di escape OSC 52 (funziona via SSH)"""
    b64_text = base64.b64encode(text.encode('utf-8')).decode('utf-8')
    sys.stdout.write(f"\033]52;c;{b64_text}\a")
    sys.stdout.flush()

def get_symbol(p_type):
    """Assegna un simbolo in base al tipo di pacchetto"""
    t = p_type.upper()
    if "DATA" in t: return "📦"
    if "ACK" in t:  return "✅"
    if "NACK" in t: return "❌"
    if "LAST" in t: return "🏁"
    if "EXPIRED" in t or "TIMEOUT" in t: return "⏰"
    if "RESET" in t: return "🔄"
    if "COMPLETE" in t: return "💎"
    return "❓"

def generate_mermaid(files):
    dataframes = []
    is_single = len(files) == 1
    
    for f in files:
        try:
            df = pd.read_csv(f)
            # Se file singolo = Primario, altrimenti cerchiamo 'tx' nel nome
            df['Origin'] = "Primario" if (is_single or "tx" in f.lower() or "pri" in f.lower()) else "Secondario"
            dataframes.append(df)
        except Exception as e:
            print(f"%% Errore file {f}: {e}")

    if not dataframes: return ""

    merged = pd.concat(dataframes).sort_values('Timestamp_us')
    
    output = [
        "sequenceDiagram",
        "    autonumber",
        "    participant A as Primario (A)",
        "    participant B as Secondario (B)"
    ]
    
    for _, row in merged.iterrows():
        origin, dir_type, p_type = row['Origin'], row['Direction'], row['Type']
        pid, seg = row['ID'], row['Segment']
        note = row['Notes'] if pd.notna(row['Notes']) else ""
        
        symbol = get_symbol(p_type)
        label = f"{symbol} {p_type} [ID:{pid} S:{seg}] {note}".strip()

        # Logica Direzione Frecce
        if (origin == "Primario" and dir_type == "TX") or (origin == "Secondario" and dir_type == "RX"):
            # Flusso A -> B
            arrow = "-->>" if "ACK" in p_type or "NACK" in p_type else "->>"
            output.append(f"    A{arrow}B: {label}")
        elif (origin == "Secondario" and dir_type == "TX") or (origin == "Primario" and dir_type == "RX"):
            # Flusso B -> A
            arrow = "-->>" if "ACK" in p_type or "NACK" in p_type else "->>"
            output.append(f"    B{arrow}A: {label}")
        else:
            # NONE (Eventi di sistema)
            output.append(f"    Note over A,B: {label}")

    return "\n".join(output)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 script.py primario.csv [secondario.csv]")
    else:
        mermaid_code = generate_mermaid(sys.argv[1:])
        print(mermaid_code)
        copy_to_clipboard(mermaid_code)
        print("\n%% [INFO] Codice Mermaid copiato negli appunti locali %%")
