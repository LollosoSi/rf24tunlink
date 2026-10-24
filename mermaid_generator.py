import pandas as pd
import sys
import base64

def copy_to_clipboard(text):
    b64_text = base64.b64encode(text.encode('utf-8')).decode('utf-8')
    sys.stdout.write(f"\033]52;c;{b64_text}\a")
    sys.stdout.flush()

def get_color(pid, origin, p_type):
    palette_a = ["#d1e9f6", "#d1f2d1", "#fff9c4"] # Flusso Primario -> Secondario
    palette_b = ["#ffecb3", "#f3e5f5", "#efebe9"] # Flusso Secondario -> Primario
    is_res = any(x in p_type.upper() for x in ["ACK", "NACK", "COMPLETE"])
    flow_a_to_b = (not is_res or "RX" in origin) if "Primario" in origin else ("RX" in origin or (is_res and "TX" in origin))
    colors = palette_a if flow_a_to_b else palette_b
    return colors[int(pid) % len(colors)]

def generate_custom_html(files):
    dataframes = []
    is_single = len(files) == 1
    for f in files:
        try:
            df = pd.read_csv(f)
            df['Origin'] = "Primario" if (is_single or "tx" in f.lower() or "pri" in f.lower()) else "Secondario"
            dataframes.append(df)
        except: continue
    
    if not dataframes: return "ERRORE: Nessun dato trovato."
    merged = pd.concat(dataframes).sort_values('Timestamp_us')

    html = """
    <!DOCTYPE html>
    <html>
    <head>
        <style>
            body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #222; color: #eee; margin: 0; padding: 20px; }
            .header { display: flex; justify-content: space-around; position: sticky; top: 0; background: #333; padding: 20px; z-index: 100; border-bottom: 2px solid #444; }
            .node-label { font-weight: bold; font-size: 1.2em; color: #fff; }
            .timeline { max-width: 900px; margin: 20px auto; position: relative; border-left: 2px dashed #444; border-right: 2px dashed #444; padding: 10px 0; }
            .row { display: flex; align-items: center; margin-bottom: 8px; height: 50px; position: relative; }
            .packet { flex-grow: 1; margin: 0 40px; padding: 8px; border-radius: 4px; color: #000; font-size: 0.85em; display: flex; justify-content: space-between; align-items: center; position: relative; }
            .arrow { position: absolute; top: 50%; width: 30px; height: 2px; background: #fff; }
            .arrow::after { content: ''; position: absolute; top: -4px; width: 0; height: 0; border-style: solid; }
            .to-right { right: -35px; } .to-right::after { right: -5px; border-width: 5px 0 5px 8px; border-color: transparent transparent transparent #fff; }
            .to-left { left: -35px; } .to-left::after { left: -5px; border-width: 5px 8px 5px 0; border-color: transparent #fff transparent transparent; }
            .note-box { width: 100%; text-align: center; background: #444; color: #fff; padding: 5px; border-radius: 4px; font-style: italic; }
            .ts { font-family: monospace; font-size: 0.7em; color: #888; width: 80px; }
        </style>
    </head>
    <body>
        <div class="header">
            <div class="node-label">PRIMARIO (A)</div>
            <div class="node-label">SECONDARIO (B)</div>
        </div>
        <div class="timeline">
    """

    for _, row in merged.iterrows():
        color = get_color(row['ID'], row['Origin'], row['Type'])
        label = f"<b>{row['Type']}</b> ID:{row['ID']} S:{row['Segment']}"
        note = f"<br><small>{row['Notes']}</small>" if pd.notna(row['Notes']) else ""
        
        # Logica Direzione
        is_a_to_b = (row['Origin'] == "Primario" and row['Direction'] == "TX") or (row['Origin'] == "Secondario" and row['Direction'] == "RX")
        is_b_to_a = (row['Origin'] == "Secondario" and row['Direction'] == "TX") or (row['Origin'] == "Primario" and row['Direction'] == "RX")
        
        html += f'<div class="row"><span class="ts">{row["Timestamp_us"] % 10000000}</span>'
        
        if is_a_to_b:
            html += f'<div class="packet" style="background:{color}; text-align: left;">{label}{note}<div class="arrow to-right"></div></div>'
        elif is_b_to_a:
            html += f'<div class="packet" style="background:{color}; text-align: right;"><div class="arrow to-left"></div>{label}{note}</div>'
        else:
            html += f'<div class="note-box">SYSTEM: {label}</div>'
            
        html += '</div>'

    html += "</div></body></html>"
    return html

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python3 script.py primario.csv [secondario.csv]")
    else:
        html_output = generate_custom_html(sys.argv[1:])
        copy_to_clipboard(html_output)
        print("%% [INFO] HTML custom generato e copiato (OSC 52) %%")