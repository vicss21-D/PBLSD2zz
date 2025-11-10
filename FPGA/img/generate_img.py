from PIL import Image

def converter_imagem_para_mif(caminho_imagem_entrada, caminho_arquivo_saida):
    """
    Lê uma imagem, converte para escala de cinza, redimensiona para 320x240
    e a salva como um arquivo .mif.

    Args:
        caminho_imagem_entrada (str): O caminho para a imagem de entrada.
        caminho_arquivo_saida (str): O caminho onde o arquivo .mif será salvo.
    """
    try:
        # Abre a imagem
        img = Image.open(caminho_imagem_entrada)
    except FileNotFoundError:
        print(f"Erro: O arquivo de imagem '{caminho_imagem_entrada}' não foi encontrado.")
        return

    # Converte a imagem para escala de cinza (8 bits por pixel)
    img_grayscale = img.convert('L')

    # Redimensiona a imagem para 320x240 pixels
    img_resized = img_grayscale.resize((320, 240))

    # Obtém os dados dos pixels
    pixels = list(img_resized.getdata())

    # Define a profundidade e a largura da memória para o arquivo .mif
    profundidade = 320 * 240 * 3
    largura = 8

    # Cria e escreve o conteúdo no arquivo .mif
    with open(caminho_arquivo_saida, 'w') as f:
        f.write(f'DEPTH = {profundidade};\n')
        f.write(f'WIDTH = {largura};\n')
        f.write('ADDRESS_RADIX = DEC;\n')
        f.write('DATA_RADIX = HEX;\n')
        f.write('CONTENT BEGIN\n')

        for i, pixel_value in enumerate(pixels):
            # Formata o valor do pixel em hexadecimal com dois dígitos
            hex_value = format(pixel_value, '02X')
            f.write(f'\t{i} : {hex_value};\n')

        f.write('END;\n')

    print(f"Arquivo .mif '{caminho_arquivo_saida}' gerado com sucesso!")

# Exemplo de uso
if __name__ == '__main__':
    # Substitua 'sua_imagem.png' pelo caminho da sua imagem
    caminho_da_imagem = 'quadrado-circulo-triangulo.jpg'
    # O arquivo .mif será salvo como 'imagem_output.mif'
    nome_arquivo_saida = 'imagem_output.mif'
    converter_imagem_para_mif(caminho_da_imagem, nome_arquivo_saida)