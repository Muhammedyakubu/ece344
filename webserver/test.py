def main():
    file = open("test.txt")
    how_many = 0
    marks = []
    txt = file.read()
    for c in range(len(txt)):
        if txt[c] == 'M':
            num = ''
            while True:
                if txt[c] == 'i':
                    num += txt[c+3]
                    num += txt[c+4]
                    break
                c += 1
            marks.append(int(num))

    every_three = 0
    num_failed = 0
    num_passed = 0
    num_14 = 0
    for num in marks:
        every_three += 1
        if num == 14:
            num_14 += 1
        if every_three % 3 == 0:
            if num_14 > 0:
                num_passed += 1
            else:
                num_failed += 1
            num_14 = 0

    print(f'Number of passed is this {num_passed}')
    print(f'Number of failed is this {num_failed}')

if __name__ == '__main__':
    main()