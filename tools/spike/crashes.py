"""Listet Abstuerze der Testinstanzen mit Fehler und Stack (RVAs): crashes.py

Manche Abstuerze legen keinen Bericht an. Dann steht die Adresse in der Windows-Ereignisanzeige (Application, ID 1000).
"""
import glob
import os
import re

SESSION = os.path.join(os.path.dirname(__file__), '..', '..', 'research', 'local', 'session')

for folder in sorted(glob.glob(os.path.join(SESSION, 'ud_*', 'Saved', 'Crashes', '*')), key=os.path.getmtime):
    context = open(os.path.join(folder, 'CrashContext.runtime-xml'), encoding='utf-8', errors='replace').read()
    stack = re.search(r'<PCallStack>(.*?)</PCallStack>', context, re.S)
    message = re.search(r'<ErrorMessage>(.*?)</ErrorMessage>', context)
    print(os.path.basename(folder), message.group(1)[:90] if message else '')
    print('   ', ' '.join(re.findall(r'\+ ([0-9a-f]+)', stack.group(1))[:14]) if stack else '')