# NPno Piano Reader — VST3 para Ableton Live (Windows)

## O que este projeto é

**Projeto-fonte funcional de instrumento sampleado**, com decodificador `.npno`,
reprodução MIDI polifônica, três bancos de amostras (attack / resonance / release),
pedal sustain, seleção por velocity-layer, transposição, saída estéreo e interface JUCE.
O motor e o decodificador foram compilados e testados **em Linux**, inclusive com o
arquivo pessoal `Black Upright Med 6.1.npno` enviado pelo usuário (462 gravações decodificadas).
Foi gerado um WAV de demonstração a partir de três notas MIDI.

**Ainda NÃO há arquivo `.vst3` compilado ou validado dentro do Ableton**. O host Windows,
o SDK do JUCE e o compilador MSVC não estão disponíveis no ambiente que criou este pacote.
Uma versão `.vst3` pode ser produzida pelo script de build ou pelo workflow GitHub Actions
incluído, mas esse processo ainda depende de execução externa e verificação real.
Não apresente esse pacote como instalador pronto.

## Como obter o VST3 no Windows

1. Instalar **Visual Studio 2022** com workload **Desktop development with C++**,
   [CMake](https://cmake.org/download/) e [Git](https://git-scm.com/download/win).
2. Extrair ZIP para pasta sem acentos e executar `build_windows.bat`.
3. Na primeira execução, CMake faz download do JUCE 8.0.10 (exige internet).
4. Copiar a pasta `NPno Piano Reader.vst3` localizada em
   `build-win/NordPianoPrototype_artefacts/Release/VST3/` para
   `C:\Program Files\Common Files\VST3`.
5. Ableton Live → Configurações → Plug-Ins → usar VST3 do sistema → **Rescan**.
6. Criar pista MIDI, arrastar **NPno Piano Reader** e escolher **Carregar piano .npno**.
7. Selecionar seu `.npno` pessoal e usar Piano Roll ou teclado MIDI.

**Alternativa CI:** publicar o conteúdo deste projeto em um repositório próprio
(com `CMakeLists.txt` na raiz) e executar o workflow
`.github/workflows/windows-vst3.yml`. O artefato será o plugin Windows, mas **não foi
executado aqui**, e qualquer erro de JUCE/SDK precisará ser resolvido no log.

## Testar somente o decodificador/motor, sem JUCE

```shell
cmake -S . -B build -DBUILD_PLUGIN=OFF
cmake --build build --parallel 4
./build/check_npno "Black Upright Med 6.1.npno"
./build/decode_npno "Black Upright Med 6.1.npno" c4.wav
./build/render_piano "Black Upright Med 6.1.npno" demo.wav
```

## Limitações importantes (não é emulação perfeita do Nord Stage)

- Os arquivos `.npno` testados usam CNSP versões 0x450/0x464 e PCM **35002 Hz**.
  Não há garantia de compatibilidade com qualquer piano ou geração da Nord.
- A curva exata de velocity e o tratamento completo dos metadados de decay,
  trim, ressonância e voicing não foram reproduzidos com precisão do hardware.
- Seleção de layer: distribuição linear através das layers presentes, uma
  **aproximação** musical e não uma alegação de equivalência com o Nord.
- Linear resampling e render de samples em RAM. Ao carregar piano grande, haverá
  uma espera de decodificação na thread da interface. Resample premium, streaming,
  curva de pedal contínua e efeitos Nord estão fora desta versão.
- Ao reabrir sessão do Ableton, o caminho anterior aparece, mas o usuário deve
  **selecionar novamente** o `.npno` no plugin. Carregamento automático foi
  propositalmente omitido porque DAWs podem restaurar estado no thread de áudio.
- Foi testado o motor e a geração WAV; **a integração JUCE não foi compilada**,
  logo a execução Windows no Ableton permanece por validar.

## Direitos e créditos

Formato e decodificação inspirados na documentação de engenharia reversa do
projeto `nord-format`/`drawbar` (BSD-3-Clause):
https://github.com/jmoo/drawbar/tree/master/crates/nord-format
https://github.com/jmoo/drawbar/blob/master/crates/nord-format/src/formats/npno/codec.rs

JUCE 8: https://juce.com/ (licenciamento específico do JUCE é aplicável).
Nenhum arquivo `.npno` ou WAV da Nord é distribuído dentro do código-fonte.
**Nord**, **Nord Stage** e os sons são marcas/conteúdo de seus titulares; o uso
pessoal não cancela as condições aplicáveis às bibliotecas de som.
Este projeto não é afiliado à Nord/Clavia.
