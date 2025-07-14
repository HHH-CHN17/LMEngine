#include "GLModel.h"

#include <QTextStream>
#include <QFile>
#include <QDebug>

CGLModel::CGLModel(QObject* parent)
	:QObject(parent)
{
	pMesh_ = new CGLMesh(this);
}

CGLModel::~CGLModel()
{
	
}

void CGLModel::loadModel(const QString& fileName)
{
	QFile objFile(fileName);
	if (!objFile.exists()) {
		qDebug() << "File nnot found";
		return;
	}

	objFile.open(QIODevice::ReadOnly);
	QTextStream input(&objFile);

	QVector<vec3f> pos;
	QVector<vec2f> uv;
	QVector<vec3f> norm;

	while (!input.atEnd())
	{
		QString str = input.readLine();
		// ���list��ʾ����һ������
		QStringList list = str.split(" ");

		if (list[0] == "#") {
			qDebug() << "This is comment:" << str;
			continue;
		}
		else if (list[0] == "mtllib") {
			qDebug() << "File with materials:" << list[1];
			continue;
		}
		else if (list[0] == "v") {  // �� v ��ͷ���ж�����һ�����ζ���
			pos.append(vec3f{ list[1].toFloat(), list[2].toFloat(), list[3].toFloat() });
			continue;
		}
		else if (list[0] == "vt") { // �� vt ��ͷ���ж�����һ����������
			uv.append(vec2f{ list[1].toFloat(), list[2].toFloat() });
			continue;
		}
		else if (list[0] == "vn") { // �� vn ��ͷ���ж�����һ�����㷨��
			norm.append(vec3f{ list[1].toFloat(), list[2].toFloat(), list[3].toFloat() });
			continue;
		}
		else if (list[0] == "f") {  // �� f ��ͷ���ж���һ��������棬ͨ��������ǰ����Ķ��㡢��������ͷ��ߵ�����������������
			for (int i = 1; i <= 3; ++i) {

				// list[i] �ĸ�ʽΪ "v/vt/vn"������ v �Ƕ���������vt ����������������vn �Ƿ�������
				QStringList attrIdx = list[i].split("/");
				vertices_.append(
					VertexAttr{
						pos[attrIdx[0].toInt() - 1],
						uv[attrIdx[1].toInt() - 1],
						norm[attrIdx[2].toInt() - 1],
						vec3f{0.0, 0.0, 0.0},
						vec3f{0.0, 0.0, 0.0}
					}
				);
				indices_.append(static_cast<GLuint>(indices_.size()));
			}
			continue;
		}
		else if (list[0] == "usemtl") {
			qDebug() << "This is used naterial:" << list[1];
		}
	}

	objFile.close();
}

void CGLModel::readvi(const QVector<VertexAttr>& vertices, const QVector<GLuint>& indices)
{

	QFile file("D:/LMEngine.txt");
	//���ļ����������򴴽�
	file.open(QIODevice::Truncate | QIODevice::WriteOnly| QIODevice::Text);
	QTextStream out(&file);
	//д���ļ���Ҫ�ַ���ΪQByteArray��ʽ
	for (int i = 0; i < vertices.size(); i++)
	{
		out << "pos: " << vertices[i].pos.x << " " << vertices[i].pos.y << " " << vertices[i].pos.z << "\n"
			<< "uv: " << vertices[i].uv.x << " " << vertices[i].uv.y << "\n"
			<< "norm: " << vertices[i].norm.x << " " << vertices[i].norm.y << " " << vertices[i].norm.z << "\n"
			<< "tan: " << vertices[i].tan.x << " " << vertices[i].tan.y << " " << vertices[i].tan.z << "\n"
			<< "bitan: " << vertices[i].bitan.x << " " << vertices[i].bitan.y << " " << vertices[i].bitan.z << "\n" << "\n";
	}
	out << "EBO: " << "\n";
	for (int i = 0; i < indices.size(); i++)
	{
		out << indices[i] << " ";
	}

	file.close();
}

void CGLModel::initialize(const QString& fileName)
{
	// ��ʼ��
	initializeOpenGLFunctions();
	loadModel(fileName);
    calculateTBN();

	static bool once = true;
	if (once)
	{
		qDebug() << vertices_.size() << " " << indices_.size();
		//readvi(vertices_, indices_);
		once = false;
	}
	

    pMesh_->initialize(vertices_, indices_);
	// pMesh_�������ڲ�����VAO,VBO,EBO�ȣ������ɹ���Ͳ���Ҫvertices_��indices_��
    vertices_.clear();
    indices_.clear();
}

void CGLModel::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightPos, const glm::vec3& viewPos)
{
	if (!pMesh_) {
		qDebug() << "Mesh is not initialized.";
		return;
	}

	pMesh_->draw(view, projection, lightPos, viewPos);
}

void CGLModel::move(const QPoint& pos, const float& scale)
{
	// ����ģ�͵�M���󼴿�
	pMesh_->move(pos, scale);
}

void CGLModel::calculateTBN()
{
	// ����������Ϊһ�飬���������������TBN���󣨼��������TBN�����������ռ�����ϵ������ɫ��������˷��߾���ת��������ռ�����ϵ��
	for (int i = 0; i < vertices_.size(); i += 3)
	{
		// �������ߺ͸����ߣ�����������TBN�����е�T��B
		float deltaU1 = vertices_[i + 1].uv.x - vertices_[i].uv.x;
		float deltaU2 = vertices_[i + 2].uv.x - vertices_[i].uv.x;

		float deltaV1 = vertices_[i + 1].uv.y - vertices_[i].uv.y;
		float deltaV2 = vertices_[i + 2].uv.y - vertices_[i].uv.y;

		glm::vec3 E1 = glm::vec3{
			vertices_[i + 1].pos.x - vertices_[i].pos.x,
			vertices_[i + 1].pos.y - vertices_[i].pos.y,
			vertices_[i + 1].pos.z - vertices_[i].pos.z
		};

		glm::vec3 E2 = glm::vec3{
			vertices_[i + 2].pos.x - vertices_[i].pos.x,
			vertices_[i + 2].pos.y - vertices_[i].pos.y,
			vertices_[i + 2].pos.z - vertices_[i].pos.z
		};

		float mu = 1 / (deltaU1 * deltaV2 - deltaV1 * deltaU2);

		glm::vec3 tagent = mu * glm::vec3{ deltaV2 * E1 - deltaV1 * E2 };

		glm::vec3 bitangent = mu * glm::vec3{ -deltaU2 * E1 - deltaU1 * E2 };

		// ��ʱ���ǵõ���tagent��bitangent�������ǣ�1. û�кͷ�����������2. û�й�һ��

		// ʩ����������
		glm::vec3 normal = {
			vertices_[i].norm.x,
			vertices_[i].norm.y,
			vertices_[i].norm.z
		};
		//											// ���ߵķ���				// ��˽����ʾtagent�ڷ��߷����ϵ�ͶӰ����
		glm::vec3 T = glm::normalize(tagent - glm::normalize(normal) * glm::dot(tagent, glm::normalize(normal)));
		glm::vec3 B = glm::normalize(glm::cross(normal, T));
		glm::vec3 N = glm::normalize(normal);

		// ע������������ռ�����ϵ
		for (int j = 0; j < 3; j++)
		{
			vertices_[i + j].tan.x = T.x;
			vertices_[i + j].tan.y = T.y;
			vertices_[i + j].tan.z = T.z;

			vertices_[i + j].bitan.x = B.x;
			vertices_[i + j].bitan.y = B.y;
			vertices_[i + j].bitan.z = B.z;

			vertices_[i + j].norm.x = N.x;
			vertices_[i + j].norm.y = N.y;
			vertices_[i + j].norm.z = N.z;
		}
	}
}